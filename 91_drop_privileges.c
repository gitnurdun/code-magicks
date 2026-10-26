/* MAGIC: shed root permanently by setting groups, gid, then uid, so a
   compromised process cannot setuid(0) back. */
#include <stdio.h>
#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <grp.h>
#include <unistd.h>
int main(void)
{
    if (geteuid() != 0) {
        printf("not root: euid=%ld\n", (long)geteuid());
        return 0;
    }
    if (setgroups(0, NULL) != 0) { perror("setgroups"); return 1; }
    if (setgid(65534) != 0) { perror("setgid"); return 1; }
    if (setuid(65534) != 0) { perror("setuid"); return 1; }
    printf("dropped: uid=%ld euid=%ld gid=%ld egid=%ld\n",
           (long)getuid(), (long)geteuid(), (long)getgid(), (long)getegid());
    errno = 0;
    if (setuid(0) == 0) puts("ERROR: regained root");
    else puts("cannot regain root");
    return 0;
}
#else
int main(void)
{
    puts("POSIX privilege dropping is not available on this platform");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   What it does
     If the process is root, it drops supplementary groups, changes its
     group id and user id to the unprivileged "nobody" account (65534),
     then tries to regain root. The final setuid(0) must fail.

   Step by step
     1. geteuid() checks effective uid. A non-root process prints its uid
        and exits; it has nothing to drop.
     2. setgroups(0, NULL) clears the supplementary group list. This must
        happen while still privileged; afterwards the process cannot
        restore groups it no longer has.
     3. setgid(65534) changes the real, effective, and saved group ids
        (for a privileged process on Linux). setuid(65534) then changes
        the real, effective, and saved user ids.
     4. Once the saved uid is no longer 0, setuid(0) fails with EPERM.
        The process is permanently unprivileged even if it is later
        compromised.
     5. The program prints the new ids and "cannot regain root". The
        order - groups, gid, uid - matters: after dropping uid, the
        process can no longer change its gid or groups.

   Output
     (when run as root)
     "dropped: uid=65534 euid=65534 gid=65534 egid=65534"
     "cannot regain root"
     (when run as a normal user)
     "not root: euid=1000"

   Why it matters
     Servers that bind a privileged port or open a protected device must
     start as root, then drop privileges before handling untrusted input.
     The classic order - setgroups, setgid, setuid, chroot, seccomp - is
     security-critical. A process that keeps a saved root id, an open
     privileged fd, or a helper thread with root can undo the drop.

   Caveats
     - Linux capabilities split root into pieces. A process may retain
       CAP_NET_BIND_SERVICE without full root via capset/prctl; dropping
       uid alone is not the whole story.
     - In a container, uid 0 may not have CAP_SETUID; the calls can fail
       even though geteuid() == 0.
     - setuid affects the whole process on Linux, but threads and signal
       handlers complicate the transition. Do it before creating threads.
     - Do not drop privileges after opening sensitive files or sockets
       unless those descriptors are meant to remain accessible.
     - chroot alone is not a security boundary; combine it with a mount
       namespace, pivot_root, seccomp, and no_new_privs.
     - This demo assumes uid/gid 65534 exists; use getpwnam("nobody") or a
       configured service account in production.
     - Windows uses tokens, integrity levels, and CreateRestrictedToken,
       not setuid; this code is POSIX-specific.
   ============================================== */
