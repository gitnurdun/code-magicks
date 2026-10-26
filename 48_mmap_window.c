/* MAGIC: map a file into the address space and let the page cache satisfy
   ordinary loads. WHY: no read syscall per byte, no extra copy. */
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
    const char *path = "mmap_demo.txt";
    const char *msg = "mmap lets the page cache be your buffer.\n";
    size_t len = strlen(msg) + 1;
#if defined(_WIN32)
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (f == INVALID_HANDLE_VALUE) { fprintf(stderr, "CreateFile failed\n"); return 1; }
    DWORD wrote;
    WriteFile(f, msg, (DWORD)len, &wrote, NULL);
    HANDLE map = CreateFileMappingA(f, NULL, PAGE_READWRITE, 0, 4096, NULL);
    char *p = map ? (char *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0) : NULL;
    if (!p) { fprintf(stderr, "mapping failed\n"); return 1; }
    printf("before: %s", p);
    p[0] = 'M';
    FlushViewOfFile(p, 1);
    printf("after:  %s", p);
    UnmapViewOfFile(p);
    CloseHandle(map);
    CloseHandle(f);
    DeleteFileA(path);
#else
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { perror("open"); return 1; }
    if (write(fd, msg, len) != (ssize_t)len) { perror("write"); return 1; }
    if (ftruncate(fd, 4096) != 0) { perror("ftruncate"); return 1; }
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    printf("before: %s", p);
    p[0] = 'M';
    msync(p, 1, MS_SYNC);
    printf("after:  %s", p);
    munmap(p, 4096);
    close(fd);
    unlink(path);
#endif
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Creates a small file, maps it into the process address space, then
     reads and edits it through a plain char pointer. The "read" is an
     ordinary load; the kernel faults in the file's page on first touch.
     Writing to a shared mapping eventually writes the file.

   Step by step
     1. The program writes a message plus its NUL terminator to a file.
        On POSIX it extends the file to one page (4096 bytes) so the
        mapping has a defined size; on Windows CreateFileMapping creates a
        page-backed section for the same range.
     2. mmap/MapViewOfFile installs a virtual-memory range whose pages are
        backed by the file. No data is copied yet. The first load of each
        page triggers a page fault; the kernel reads the page from the
        page cache (or disk) and maps it into the process.
     3. printf reads the mapped bytes exactly like heap or stack memory;
        there is no read() call and no user-space buffer copy.
     4. `p[0] = 'M'` changes the mapping. Because it is MAP_SHARED /
        PAGE_READWRITE, the change is visible in the page cache and is
        written back to the file. msync/FlushViewOfFile requests writeback;
        the OS may delay it otherwise.
     5. The "after" line shows Mmap where "mmap" was, proving that the
        pointer aliases the file's contents.

   Output
     "before: mmap lets the page cache be your buffer."
     "after:  Mmap lets the page cache be your buffer."

   Why it matters
     Memory mapping turns file I/O into memory access. It avoids copying
     between kernel and user buffers, lets the page cache act as a shared
     buffer, and makes random access natural. Databases, loaders, and
     game asset streaming use mappings for large read-mostly files.
     MAP_PRIVATE adds copy-on-write for cheap snapshots.

   Caveats
     - The mapping covers whole pages; the file may be smaller. Accessing
       beyond the last valid page can raise SIGBUS (POSIX) or an access
       violation (Windows), even if the address looks mapped.
     - A shared mapping is not a transaction. msync does not guarantee
       that every disk or remote filesystem has committed the data.
     - Mapping a file that another process truncates is dangerous; the
       mapping can outlive its backing storage.
     - 32-bit processes have limited address space; map windows, not
       entire multi-gigabyte files.
     - Windows file mappings and POSIX mmap have different flags,
       alignment rules, and error reporting; this demo hides them behind
       #ifdef, not behind a production abstraction.
   ============================================== */
