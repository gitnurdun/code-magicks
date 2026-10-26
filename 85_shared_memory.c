/* MAGIC: map the same physical page twice; a write through one virtual
   address is visible through the other with no copy. */
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
int main(void)
{
    const char *msg = "shared page";
#if defined(_WIN32)
    HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, 4096, "magic_shm_demo");
    if (!map) { fprintf(stderr, "CreateFileMapping failed\n"); return 1; }
    char *a = (char *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    char *b = (char *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    if (!a || !b) { fprintf(stderr, "MapViewOfFile failed\n"); return 1; }
    memcpy(a, msg, strlen(msg) + 1);
    printf("second view: %s\n", b);
    UnmapViewOfFile(a);
    UnmapViewOfFile(b);
    CloseHandle(map);
#else
    int fd = shm_open("/magic_shm_demo", O_CREAT | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, 4096) != 0) { perror("ftruncate"); return 1; }
    char *a = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    char *b = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (a == MAP_FAILED || b == MAP_FAILED) { perror("mmap"); return 1; }
    memcpy(a, msg, strlen(msg) + 1);
    printf("second view: %s\n", b);
    munmap(a, 4096);
    munmap(b, 4096);
    close(fd);
    shm_unlink("/magic_shm_demo");
#endif
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Creates one shared-memory object and maps it into the process twice.
     It writes through the first mapping and reads the same bytes through
     the second, proving that both virtual addresses refer to the same
     physical pages.

   Step by step
     1. On Windows, CreateFileMapping with INVALID_HANDLE_VALUE creates a
        named page-file-backed section. On POSIX, shm_open creates a named
        object under /dev/shm and ftruncate gives it one page.
     2. MapViewOfFile / mmap installs a virtual-address range backed by
        that object. Calling it twice produces two different virtual
        addresses, a and b, but the kernel maps both to the same physical
        page.
     3. memcpy(a, "shared page", 12) writes through a. The CPU's cache
        coherence protocol makes the write visible through b; no syscall
        or copy is needed.
     4. printf("%s", b) reads through the second mapping and prints the
        message. The observed sharing is at page granularity.
     5. Unmapping and closing releases the views. shm_unlink removes the
        POSIX name; the Windows section disappears when the last handle
        closes.

   Output
     "second view: shared page"

   Why it matters
     Shared memory is the fastest IPC: both processes pay one mapping cost
     and then exchange data through ordinary loads and stores. Databases
     use it for buffer pools, browsers for renderer communication, and
     game engines for large asset caches. It is also the basis of
     copy-on-write, memory-mapped files, and some zero-copy protocols.

   Caveats
     - There is no synchronisation here. Two processes writing the same
       bytes race; use atomics, mutexes in shared memory, or a message
       protocol.
     - Shared memory is not a message queue: you must manage offsets,
       sizes, versions, and lifetime yourself.
     - POSIX shared-memory names persist until shm_unlink; a crashed
       process can leave stale objects. Windows named sections have
       similar namespace and security concerns.
     - Permissions matter: a world-writable segment is an attack surface.
       Use restrictive modes and validate all data from another process.
     - Mapping the same page twice is not a supported way to get two
       independent copies; use MAP_PRIVATE or memcpy when you want COW.
     - Address-space limits and alignment differ between platforms; 4096
       is the common page size, not a universal constant.
   ============================================== */
