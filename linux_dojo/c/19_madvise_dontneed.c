/* MAGIC: MADV_DONTNEED discards private anonymous pages now; the next
   read faults in the shared zero page. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static long resident_pages(void)
{
    FILE *f;
    long total, resident;

    f = fopen("/proc/self/statm", "r");
    if (f == NULL)
        return -1;
    if (fscanf(f, "%ld %ld", &total, &resident) != 2)
        resident = -1;
    if (fclose(f) != 0)
        resident = -1;
    return resident;
}

int main(void)
{
    const size_t len = 1UL << 20;
    long page = sysconf(_SC_PAGESIZE);
    long before, after;
    unsigned char *p;
    size_t off;
    int zero;

    if (page <= 0) {
        printf("skipped: cannot determine page size\n");
        return 0;
    }
    p = mmap(NULL, len, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    /* Dirty one byte in every page so the mapping is really populated. */
    for (off = 0; off < len; off += (size_t)page)
        p[off] = 0xAA;

    before = resident_pages();
    if (before < 0) {
        printf("skipped: /proc/self/statm unavailable\n");
        munmap(p, len);
        return 0;
    }

    if (madvise(p, len, MADV_DONTNEED) != 0) {
        perror("madvise");
        munmap(p, len);
        return 1;
    }
    after = resident_pages();
    if (after < 0) {
        printf("skipped: /proc/self/statm unavailable\n");
        munmap(p, len);
        return 0;
    }

    zero = (p[0] == 0);
    if (munmap(p, len) != 0) {
        perror("munmap");
        return 1;
    }
    printf("resident_before>after=%s zero_filled=%s\n",
           before > after ? "yes" : "no", zero ? "yes" : "no");
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Maps 1 MB of private anonymous memory, writes one nonzero byte in
     every 4 KB page, reads the resident-page count from
     /proc/self/statm, calls madvise(MADV_DONTNEED), reads the count
     again, and reads the first byte.  The resident count drops and the
     byte reads back as zero, proving the pages were discarded and a
     fresh zero page was faulted in.

   Step by step
     1. mmap(MAP_PRIVATE | MAP_ANONYMOUS) creates a zero-filled mapping
        with no file behind it.  It is demand-paged: no physical page
        exists until a page is touched.
     2. The loop writes 0xAA at every page boundary.  Each write faults
        in an anonymous page, so the mapping becomes resident.
     3. /proc/self/statm fields are: size, resident, shared, text, lib,
        data, dt (all in pages).  field 2 is the resident set size; the
        code reads just the first two fields.
     4. MADV_DONTNEED on private anonymous memory tells the kernel it
        may discard the pages immediately.  The kernel drops the page
        table entries and frees the pages; RSS falls.
     5. Reading p[0] afterwards causes a minor fault.  For anonymous
        memory the kernel maps the shared, read-only zero page, so the
        value is 0 - even though the original byte was 0xAA.
     6. The program prints two booleans, then unmaps the region.

   Output
     resident_before>after=yes zero_filled=yes

   Why it matters
     This is the cheap way for a cache or arena to return memory to the
     system without freeing the virtual range.  A long-lived process can
     keep its address space stable while releasing the backing pages,
     then let them fault back as needed.  It is also how memory
     allocators purge free spans and how big sparse buffers are reset
     to zero without a memset.

   Caveats
     - MADV_DONTNEED is not msync(2).  msync writes dirty MAP_SHARED
       file pages back to storage; MADV_DONTNEED drops page mappings.
       On a MAP_SHARED file mapping the data is still in the page cache
       (and is not lost if dirty), so a later read usually sees the file,
       not zeros.  Only private anonymous memory is reset to zero this
       way.
     - It is a hint: the kernel may choose to keep pages.  For private
       anonymous memory it is reliable in practice.
     - statm counts every resident page in the process, including libc
       text and stacks, so only the difference matters; stdio's buffer
       may add a few pages between the two reads.
     - A read after DONTNEED faults the zero page but counts as
       resident only once per process until re-dirtied.  The first byte
       read is a fresh fault; a write would allocate a private copy.
     - Transparent huge pages change the arithmetic: the 1 MB mapping
       may use large pages, and DONTNEED may split them.  The comparison
       before > after still holds, but the exact count is not portable.
     - MADV_FREE is the lazier variant: it marks pages reusable and
       reclaims them under pressure; MADV_DONTNEED acts now.
   ============================================== */
