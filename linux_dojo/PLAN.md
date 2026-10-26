# linux_dojo — house style and rules

A Linux deep-dive companion to the 100-entry collection in the parent directory.
Every file is a standalone program followed by an explanation. The subject is the
Linux contract: kernel ABIs, /proc and /sys, namespaces, zero-copy I/O, modern
syscalls, and the shell as a systems language.

## Hard rules

1. **One file = one trick.** The code must be readable in under two minutes.
   Code portion (excluding the explanation) is at most ~80 lines. Whole file at
   most ~140 lines. No TODOs, no pseudo-code, no missing functions.
2. **Standalone.** Standard library only. No third-party packages. One-line
   kernel headers and libc are fine. No repository imports.
3. **Deterministic and safe.** Output must be stable across runs. No network
   access to the internet, no writes outside a `mktemp`/`tempfile` directory or
   the current directory, no `rm -rf` on an unchecked path, no fork bombs.
4. **Graceful failure.** If a syscall needs root, a newer kernel, a sysctl, or an
   optional tool, detect the condition, print one clear
   `skipped: <reason>` line, and exit 0. Never leave the reader with a raw EPERM
   traceback. The real implementation must still be present.
5. **Accurate explanations.** The explanation must say what the contract is,
   where the trick breaks, and which kernel version or config it needs. No
   marketing claims. Measure before calling something faster.
6. **No overlap** with the parent collection: no second Duff's device, intrusive
   list, seqlock, generic epoll-ET, sendfile, basic seccomp/chroot/uid-drop,
   pipe tutorial, shared-memory tutorial, or generic signal handler. The
   assigned topics are already distinct; do not drift into a neighbour's topic.
7. **Names are exact.** Use the assigned number and slug. Lowercase slugs.

## File format

### Bash (`linux_dojo/bash/NN_slug.sh`)

```bash
#!/usr/bin/env bash
# MAGIC: one sentence naming the trick.
set -euo pipefail

# ... code ...

# ================= EXPLANATION =================
# What it does
#   ...
# Step by step
#   1. ...
#   2. ...
# Output
#   ...
# Why it matters
#   ...
# Caveats
#   - ...
# ==============================================
```

- Quote every expansion that can contain whitespace.
- Prefer `[[ ]]`, `printf`, `read -r`, `local`.
- `set -euo pipefail` is the default, but a script whose whole point is an
  errexit pitfall may omit it deliberately and must say so.
- Avoid GNU-only flags in a POSIX demo; this is a Linux collection, so GNU is
  allowed, but say when a flag is GNU-specific.
- Expected output must be exact and stable. Do not print timings, PIDs, or
  random values without normalising them.

### C (`linux_dojo/c/NN_slug.c`)

```c
/* MAGIC: one sentence naming the trick. */
#define _GNU_SOURCE
#include <...>

int main(void)
{
    ...
}

/* ================= EXPLANATION =================
   What it does
     ...
   Step by step
     1. ...
   Output
     ...
   Why it matters
     ...
   Caveats
     - ...
   ============================================== */
```

- Linux-only is fine. Add a `#if defined(__linux__) ... #else` one-line stub
  only if it does not obscure the real code; otherwise state Linux-only in the
  header. Do not write a Windows implementation.
- Check every syscall return. Close every fd. Free every allocation.
- If a header may be missing on older systems, guard the feature with
  `#if defined(__NR_xxx)` or `__has_include`, not with a fake prototype.
- Print kernel-version-dependent detail only after a successful syscall.

### Python (`linux_dojo/python/NN_slug.py`)

```python
#!/usr/bin/env python3
"""MAGIC: one sentence naming the trick."""
...
# ================= EXPLANATION =================
# What it does
#   ...
# ...
# ==============================================
```

- Python 3.8+, stdlib only. `ctypes` is allowed and encouraged for raw ABIs.
- `sys.platform.startswith("linux")` guard with a clean skip.
- Clean up temp files with `try/finally` or `atexit`.
- `struct` over hand-rolled slices where it clarifies the ABI.

### Awk (`linux_dojo/awk/NN_slug.awk`)

Use `#!/usr/bin/awk -f`, `# MAGIC:`, and a trailing `#` explanation block.
State whether the script needs gawk or works in POSIX awk.

## Review checklist

- [ ] File begins with the MAGIC line.
- [ ] Code compiles or parses; no unused includes; no warnings by inspection.
- [ ] Every syscall/return value is checked.
- [ ] Resources are released (fd, mmap, thread, temp dir).
- [ ] Output block matches what the code actually prints.
- [ ] Caveats mention the real failure mode and the kernel/config prerequisite.
