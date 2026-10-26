/* MAGIC: find the virtual-memory area that contains a local variable by
   parsing /proc/self/maps. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    unsigned char local = 0;
    uintptr_t target = (uintptr_t)&local;
    int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("skipped: cannot open /proc/self/maps (%s)\n", strerror(errno));
        return 0;
    }
    FILE *maps = fdopen(fd, "r");
    if (maps == NULL) {
        printf("skipped: cannot read /proc/self/maps (%s)\n", strerror(errno));
        close(fd);
        return 0;
    }

    char *line = NULL;
    size_t cap = 0;
    int found = 0;
    unsigned long start = 0, end = 0;
    char perms[5] = "", name[256] = "";
    long page = sysconf(_SC_PAGESIZE);
    unsigned long pg = page > 0 ? (unsigned long)page : 4096UL;

    while (getline(&line, &cap, maps) != -1) {
        unsigned long offset, inode;
        unsigned int maj, min;
        perms[0] = name[0] = '\0';
        int n = sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %255s",
                       &start, &end, perms, &offset, &maj, &min,
                       &inode, name);
        if (n >= 7 && target >= start && target < end) {
            found = 1;
            break;
        }
    }
    if (ferror(maps)) {
        puts("skipped: error reading /proc/self/maps");
        free(line);
        fclose(maps);
        return 0;
    }
    fclose(maps);
    free(line);

    if (!found) {
        puts("found=0");
        return 1;
    }
    unsigned long in_map = (unsigned long)(target - start);
    printf("found=1 perms=%s is_stack=%d mapping_page=%lu page_offset=0x%lx\n",
           perms, strcmp(name, "[stack]") == 0,
           in_map / pg, in_map % pg);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Reads this process's own /proc/self/maps file, parses each VMA
     (virtual-memory-area) line, and looks for the range that contains
     the address of main's local variable. It prints the matching
     permissions, whether the VMA is named "[stack]", and where inside
     the mapping the variable lies.

   Step by step
     1. `&local` produces a real stack address; taking its address forces
        the compiler to give it storage in main's stack frame.
     2. open() plus fdopen() gives a stdio stream over /proc/self/maps.
        That file is generated on read, one text line per VMA, for
        example:
          7ffd...000-7ffd...000 rw-p 00000000 00:00 0 [stack]
     3. getline() returns one line at a time. sscanf() parses:
        start-end, 4 permission characters, file offset, major:minor,
        inode, and an optional path. Unnamed mappings produce the first
        seven fields, so the test is `n >= 7`.
     4. The target is compared with `start <= target < end`. End is
        exclusive; adjacent VMAs do not overlap.
     5. Once found, the code keeps the line's perms/name, and computes
        the byte distance from the VMA start. `mapping_page` is that
        distance divided by the page size; `page_offset` is the offset
        inside that page. Both are relationships, not raw pointers.
     6. The stream is closed and the getline() buffer is freed. If procfs
        is unavailable, the program prints one skipped: line and exits 0.

   Output
     found=1 perms=rw-p is_stack=1 mapping_page=<n> page_offset=0x<off>
     The mapping page and offset depend on the stack layout and ASLR.

   Why it matters
     /proc/self/maps is the process's view of its address space: text,
     data, heap, stack, shared libraries, anonymous mmaps, and guard
     pages. Tools such as loaders, profilers, crash reporters, and
     sandbox checkers use it to discover permissions and placements.
     ASLR randomizes mapping bases (including the stack) at exec time,
     so the absolute address changes while the offset inside a page is
     much more stable. Inspecting the containing VMA turns a raw address
     into a meaningful object.

   Caveats
     - The file is a snapshot, produced as it is read. The stack can grow
       and mappings can be added or removed immediately afterwards; it is
       not a live view and not a lock.
     - The main thread's stack is normally "[stack]", but secondary
       thread stacks are anonymous or named by the runtime. A variable in
       another thread will not match main's stack VMA.
     - Paths with spaces are truncated by `%s`; this parser only needs
       the name field for the stack check.
     - Absolute addresses and some offsets vary with ASLR, environment,
       compiler, and kernel. Do not use this output as a stable key.
     - procfs must be mounted. Some sandboxes restrict /proc even for
       self; the open failure is reported as skipped.
     - Permissions are the VMA's current protection bits, not proof that
       every page in it is resident.
   ============================================== */
