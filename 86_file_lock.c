/* MAGIC: a byte-range lock is owned by the kernel, not by a flag in your
   process; closing the descriptor releases it. */
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
int main(void)
{
    const char *path = "lock_demo.txt";
#if defined(_WIN32)
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { fprintf(stderr, "open failed\n"); return 1; }
    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof ov);
    if (!LockFileEx(f, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &ov)) {
        fprintf(stderr, "LockFileEx failed\n");
        CloseHandle(f);
        return 1;
    }
    puts("locked");
    UnlockFileEx(f, 0, 1, 0, &ov);
    puts("unlocked");
    CloseHandle(f);
    DeleteFileA(path);
#else
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) { perror("open"); return 1; }
    struct flock fl;
    memset(&fl, 0, sizeof fl);
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if (fcntl(fd, F_SETLK, &fl) != 0) { perror("fcntl lock"); close(fd); return 1; }
    puts("locked");
    fl.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &fl);
    puts("unlocked");
    close(fd);
    unlink(path);
#endif
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Opens a file and takes an exclusive lock on its first byte, then
     releases the lock and closes the file. The lock is a kernel object:
     other processes that follow the same protocol will block or fail
     while it is held.

   Step by step
     1. The file is opened read/write, creating it if necessary. On
        Windows the share mode allows other readers and writers to open
        it; locking is separate from opening.
     2. LockFileEx with LOCKFILE_EXCLUSIVE_LOCK and length 1 locks one
        byte starting at offset 0. On POSIX, fcntl(F_SETLK) with
        l_type = F_WRLCK does the same. F_SETLK is non-blocking and
        returns EACCES/EAGAIN if another process holds a conflicting lock.
     3. The process prints "locked" while it owns the byte range. A second
        process attempting a write lock on that byte would fail (F_SETLK)
        or sleep (F_SETLKW / blocking LockFileEx).
     4. UnlockFileEx / fcntl(F_UNLCK) releases the range. Closing the
        descriptor also releases all locks the process holds on that file,
        which is a common source of surprises.
     5. The demo prints "unlocked", closes the file, and removes it.

   Output
     "locked"
     "unlocked"

   Why it matters
     File locks coordinate independent processes without shared memory or
     a central server. They are used for database lock files, package
     managers, editor swap files, and single-instance applications. Byte-
     range locks let unrelated records in one file be locked independently.

   Caveats
     - POSIX fcntl locks are advisory: a process that ignores them can
       still read/write the file. Windows byte-range locks are mandatory
       for conflicting I/O, but not all APIs honour them identically.
     - fcntl locks are per-process, not per-thread, and closing any
       descriptor for the file drops all locks the process holds on it.
     - flock and fcntl locks have different semantics on some systems;
       do not mix them.
     - Locks over NFS/SMB may be emulated or unreliable; check the
       filesystem and mount options.
     - Two locks acquired in different orders can deadlock. Use timeouts
       and a global lock order.
     - A crashed process releases its locks when the kernel closes its
       descriptors, but the protected data may be left inconsistent; pair
       locks with atomic writes or a journal.
   ============================================== */
