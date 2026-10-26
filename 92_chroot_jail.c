/* MAGIC: chroot changes the root of path resolution for this process;
   it is a filesystem view, not by itself a security boundary. */
#include <stdio.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
int main(void)
{
    if (geteuid() != 0) {
        puts("chroot requires root; not attempted");
        return 0;
    }
    mkdir("/tmp/magick_jail", 0755);
    if (chroot("/tmp/magick_jail") != 0) { perror("chroot"); return 1; }
    if (chdir("/") != 0) { perror("chdir"); return 1; }
    char cwd[256];
    if (getcwd(cwd, sizeof cwd) != NULL) printf("jailed cwd=%s\n", cwd);
    puts("jail active");
    return 0;
}
#else
int main(void)
{
    puts("chroot is not available on this platform");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   What it does
     If run as root, creates a directory and makes it the process's root
     with chroot(2), then changes the working directory to "/" inside the
     new root. Paths such as /etc/passwd now resolve under
     /tmp/magick_jail/etc/passwd.

   Step by step
     1. geteuid() checks for root; chroot requires CAP_SYS_CHROOT on Linux.
        A non-root process prints a message and does nothing.
     2. mkdir creates the jail directory if it does not exist. A real jail
        would already contain the libraries, configuration, and device
        nodes the confined program needs.
     3. chroot("/tmp/magick_jail") changes the process's root directory.
        It does not change the current working directory; a process
        standing outside the new root could still use relative paths and
        file descriptors to escape.
     4. chdir("/") moves to the new root, closing that relative-path
        escape. getcwd prints "/" because the kernel now reports paths
        relative to the new root.
     5. The process prints "jail active". A real service would then drop
        privileges, install seccomp, and exec the confined program.

   Output
     (when run as root)
     "jailed cwd=/"
     "jail active"
     (when run as a normal user)
     "chroot requires root; not attempted"

   Why it matters
     chroot is the original Unix filesystem jail and the conceptual
     ancestor of containers. It is used by FTP servers, build sandboxes,
     and boot environments. Understanding its limits explains why modern
     containers need mount namespaces, pivot_root, capabilities, and
     seccomp rather than chroot alone.

   Caveats
     - chroot is not a security boundary. A root process inside the jail
       can often escape by chrooting again, using an open fd to a directory
       outside, or mounting filesystems.
     - Even a non-root process may escape if it retains an open directory
       fd outside the new root; close or restrict them before chrooting.
     - Drop privileges after chroot, not before: chroot itself needs
       privilege. The order is chroot -> chdir -> setgroups -> setgid ->
       setuid -> seccomp.
     - The jail must contain a working dynamic loader, libc, and any
       runtime files; otherwise exec fails.
     - Linux mount namespaces and pivot_root give a much stronger
       boundary; seccomp and Landlock restrict what the jailed process can
       do even with a valid filesystem view.
     - This demo creates an empty jail and does not exec anything, so it
       only demonstrates the path change.
   ============================================== */
