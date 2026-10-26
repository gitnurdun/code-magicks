/* MAGIC: drive one io_uring NOP through the raw shared-memory submission
   and completion rings, without liburing. */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/io_uring.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void)
{
    struct io_uring_params params;
    memset(&params, 0, sizeof params);
    int fd = (int)syscall(SYS_io_uring_setup, 8, &params);
    if (fd < 0) {
        if (errno == ENOSYS || errno == EINVAL ||
            errno == EPERM || errno == EACCES) {
            printf("skipped: io_uring unavailable (%s)\n", strerror(errno));
            return 0;
        }
        perror("io_uring_setup");
        return 1;
    }

    int single = !!(params.features & IORING_FEAT_SINGLE_MMAP);
    size_t sq_ring_sz = params.sq_off.array +
                        params.sq_entries * sizeof(unsigned);
    size_t cq_ring_sz = params.cq_off.cqes +
                        params.cq_entries * sizeof(struct io_uring_cqe);
    size_t sq_map_sz = sq_ring_sz;
    if (single && cq_ring_sz > sq_map_sz)
        sq_map_sz = cq_ring_sz;

    void *sq_ring = mmap(NULL, sq_map_sz, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, IORING_OFF_SQ_RING);
    if (sq_ring == MAP_FAILED) {
        perror("mmap SQ ring");
        close(fd);
        return 1;
    }
    void *cq_ring = sq_ring;
    if (!single) {
        cq_ring = mmap(NULL, cq_ring_sz, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, IORING_OFF_CQ_RING);
        if (cq_ring == MAP_FAILED) {
            perror("mmap CQ ring");
            munmap(sq_ring, sq_map_sz);
            close(fd);
            return 1;
        }
    }
    size_t sqes_sz = params.sq_entries * sizeof(struct io_uring_sqe);
    void *sqes_map = mmap(NULL, sqes_sz, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, IORING_OFF_SQES);
    if (sqes_map == MAP_FAILED) {
        perror("mmap SQEs");
        if (!single)
            munmap(cq_ring, cq_ring_sz);
        munmap(sq_ring, sq_map_sz);
        close(fd);
        return 1;
    }

    unsigned *sq_tail = (unsigned *)((char *)sq_ring + params.sq_off.tail);
    unsigned *sq_mask = (unsigned *)((char *)sq_ring + params.sq_off.ring_mask);
    unsigned *sq_array = (unsigned *)((char *)sq_ring + params.sq_off.array);
    unsigned *cq_head = (unsigned *)((char *)cq_ring + params.cq_off.head);
    unsigned *cq_tail = (unsigned *)((char *)cq_ring + params.cq_off.tail);
    unsigned *cq_mask = (unsigned *)((char *)cq_ring + params.cq_off.ring_mask);
    struct io_uring_cqe *cqes =
        (struct io_uring_cqe *)((char *)cq_ring + params.cq_off.cqes);
    struct io_uring_sqe *sqes = sqes_map;

    unsigned tail = __atomic_load_n(sq_tail, __ATOMIC_ACQUIRE);
    unsigned index = tail & *sq_mask;
    memset(&sqes[index], 0, sizeof sqes[index]);
    sqes[index].opcode = IORING_OP_NOP;
    sqes[index].user_data = 0x1234;
    sq_array[index] = index;
    __atomic_store_n(sq_tail, tail + 1, __ATOMIC_RELEASE);

    int rc = 1;
    int submitted = (int)syscall(SYS_io_uring_enter, fd, 1, 1,
                                 IORING_ENTER_GETEVENTS, NULL, 0);
    if (submitted != 1) {
        fprintf(stderr, "io_uring_enter: %s\n",
                submitted < 0 ? strerror(errno) : "submitted 0");
        goto out;
    }

    unsigned head = __atomic_load_n(cq_head, __ATOMIC_ACQUIRE);
    unsigned seen_tail = head;
    for (unsigned long spins = 0; spins < 1000000000UL; spins++) {
        seen_tail = __atomic_load_n(cq_tail, __ATOMIC_ACQUIRE);
        if (seen_tail != head)
            break;
    }
    if (seen_tail == head) {
        fprintf(stderr, "io_uring completion timed out\n");
        goto out;
    }
    struct io_uring_cqe *cqe = &cqes[head & *cq_mask];
    if (cqe->res != 0 || cqe->user_data != 0x1234) {
        fprintf(stderr, "io_uring completion: res=%d user_data=0x%llx\n",
                cqe->res, (unsigned long long)cqe->user_data);
        goto out;
    }
    __atomic_store_n(cq_head, head + 1, __ATOMIC_RELEASE);
    printf("submitted=1 completed=1 res=0 user_data=0x1234\n");
    rc = 0;

out:
    if (munmap(sqes_map, sqes_sz) != 0)
        rc = 1;
    if (!single && munmap(cq_ring, cq_ring_sz) != 0)
        rc = 1;
    if (munmap(sq_ring, sq_map_sz) != 0)
        rc = 1;
    if (close(fd) != 0)
        rc = 1;
    return rc;
}

/* ================= EXPLANATION =================
   What it does
     Calls io_uring_setup(2) directly, maps the submission queue (SQ)
     ring, completion queue (CQ) ring, and submission-queue entries
     (SQEs), submits one IORING_OP_NOP, waits for its completion, and
     prints the normalised result. No liburing is used.

   Step by step
     1. io_uring_setup(8, &params) creates the ring. The kernel fills
        params with entry counts and byte offsets for every ring field.
        On x86-64 the syscall number is 425; SYS_io_uring_setup is used.
     2. The ring size is not a fixed struct: it is the maximum offset
        reached by the fields. SQ ends at sq_off.array plus sq_entries
        unsigned array slots; CQ ends at cq_off.cqes plus cq_entries
        completion entries.
     3. IORING_FEAT_SINGLE_MMAP means the kernel placed both rings in one
        mapping. Map it once at IORING_OFF_SQ_RING with the larger size
        and use that base for both. Otherwise map SQ_RING and CQ_RING
        separately. SQEs are always a separate IORING_OFF_SQES mapping.
     4. The offsets in params are byte offsets, so each pointer is
        `base + offset`. sq.array[] is an indirection table: its value
        selects the SQE slot. Only the tail is written by user space.
     5. Fill SQE[tail & mask] with a zeroed NOP and user_data 0x1234.
        Publish the index in sq.array, then store tail+1 with release
        ordering. The release store makes the SQE and array writes
        visible before the kernel can observe the new tail.
     6. io_uring_enter(fd, 1, 1, IORING_ENTER_GETEVENTS, NULL, 0) asks
        the kernel to consume one SQE and wait for at least one
        completion. The kernel writes the CQE and advances the CQ tail.
     7. Busy-wait, with a finite 1e9-iteration bound, for an acquire load
        of cq_tail to differ from our cq_head. Read cqes[head & mask],
        check res and user_data, then advance cq_head with a release store
        so the kernel can reuse the slot. Unmap all three mappings and
        close the ring fd.

   Output
     submitted=1 completed=1 res=0 user_data=0x1234

   Why it matters
     io_uring is a shared-memory completion model: normally one enter
     syscall submits a batch and reaps many results, instead of one
     syscall per I/O. In this demo one producer and one consumer move the
     indices with explicit memory ordering. NOP is the cleanest way to
     see the ABI before touching files, sockets, or fixed buffers.

   Caveats
     - io_uring_setup/enter arrived in Linux 5.1. Headers and
       IORING_FEAT_SINGLE_MMAP (5.4) must match the running kernel; older
       kernels return ENOSYS or EINVAL and the program skips.
     - Some seccomp policies block the io_uring syscalls, and
       kernel.io_uring_disabled can disable them system-wide.
     - The ring sizes and offsets are ABI, not guesses. Hard-coding them
       breaks across kernel versions and architectures.
     - Memory barriers are required. Plain stores can be reordered before
       the SQE is visible; this uses __ATOMIC_RELEASE for both tails and
       __ATOMIC_ACQUIRE for the CQ head/tail.
     - This demo has one outstanding SQE and no overflow handling. A real
       loop must bound submissions by sq_entries, watch CQ overflow, and
       handle short io_uring_enter() returns and EINTR.
     - IORING_OP_NOP completes locally in the kernel; it does no I/O and
       is not a latency benchmark.
   ============================================== */
