#!/usr/bin/env python3
"""MAGIC: the kernel keyring stores named payloads in a per-process keyring,
retrievable by key serial through keyctl(2)."""
import ctypes
import os
import platform
import sys

KEY_SPEC_PROCESS_KEYRING = -2
KEYCTL_READ = 11
SYSCALLS = {
    "x86_64": (248, 250),   # add_key, keyctl
    "aarch64": (217, 219),
    "arm64": (217, 219),
}


def main():
    if not sys.platform.startswith("linux"):
        print("skipped: non-Linux platform")
        return 0
    numbers = SYSCALLS.get(platform.machine())
    if numbers is None:
        print("skipped: unknown architecture for keyring syscalls")
        return 0
    add_key_nr, keyctl_nr = numbers

    libc = ctypes.CDLL(None, use_errno=True)
    libc.syscall.restype = ctypes.c_long
    payload = b"keyring secret"
    key = libc.syscall(
        ctypes.c_long(add_key_nr),
        ctypes.c_char_p(b"user"),
        ctypes.c_char_p(b"linux_dojo_key"),
        ctypes.c_char_p(payload),
        ctypes.c_size_t(len(payload)),
        ctypes.c_int(KEY_SPEC_PROCESS_KEYRING),
    )
    if key < 0:
        err = ctypes.get_errno()
        print(f"skipped: add_key failed ({os.strerror(err)})")
        return 0

    buf = ctypes.create_string_buffer(64)
    n = libc.syscall(
        ctypes.c_long(keyctl_nr),
        ctypes.c_int(KEYCTL_READ),
        ctypes.c_long(key),
        buf,
        ctypes.c_size_t(len(buf)),
    )
    if n < 0:
        err = ctypes.get_errno()
        print(f"skipped: keyctl read failed ({os.strerror(err)})")
        return 0

    print("added=yes")
    print(f"data={buf.raw[:n].decode()}")
    print(f"serial_positive={'yes' if key > 0 else 'no'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# ================= EXPLANATION =================
# What it does
#   Adds a "user" key named linux_dojo_key to the process keyring, then reads
#   its payload back with keyctl(KEYCTL_READ). No filesystem object is created.
#
# Step by step
#   1. add_key(type, description, payload, plen, keyring) needs a keyring id.
#      KEY_SPEC_PROCESS_KEYRING (-2) is a magic id for the caller's process
#      keyring, created on demand.
#   2. The kernel copies the payload into a key object and returns a positive
#      serial number. The key is owned by the caller's user namespace and
#      subject to quota.
#   3. keyctl(KEYCTL_READ, serial, buf, buflen) copies the payload out and
#      returns its length. The buffer must be at least that large.
#   4. The syscall numbers differ by architecture; the table covers the two
#      common 64-bit Linux ABIs and skips otherwise.
#
# Output
#   added=yes
#   data=keyring secret
#   serial_positive=yes
#
# Why it matters
#   Keyrings hold secrets that should not sit in a file or an environment
#   variable: TLS session keys, dm-crypt keys, Kerberos tickets. The kernel can
#   keep them in memory, restrict them by key permissions, and let a process
#   find them by name without passing the secret through argv. The process,
#   session, and user keyrings form a small hierarchy with different lifetimes.
#
# Caveats
#   - Keyrings are not a vault. A process with ptrace access or root can read
#     them, and a core dump or /proc memory read can expose payloads.
#   - Payload size is limited (32 KiB for user keys in current kernels) and
#     key quota is per user; EDQUOT is a real failure mode.
#   - Key ownership and permissions are checked against the caller's fsuid and
#     user namespace; behavior in a container depends on the namespace setup.
#   - KEY_SPEC_PROCESS_KEYRING dies with the process. Use the session or user
#     keyring for a longer lifetime, and remember that "longer" is still not
#     persistent across reboot.
#   - The syscall table is partial; an unknown architecture skips rather than
#     guessing a number and invoking the wrong syscall.
# ==============================================
