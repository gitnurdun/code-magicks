/* MAGIC: perf_event_open exposes the CPU's instruction counter as a readable file descriptor. */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static int perf_open(struct perf_event_attr *attr)
{
    return (int)syscall(SYS_perf_event_open, attr, 0, -1, -1, 0);
}

int main(void)
{
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof pe);
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof pe;
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;

    int fd = perf_open(&pe);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM || errno == ENOENT) {
            printf("skipped: perf_event_paranoid\n");
            return 0;
        }
        if (errno == ENOSYS) {
            printf("skipped: perf_event_open\n");
            return 0;
        }
        perror("perf_event_open");
        return 1;
    }

    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) < 0 ||
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        perror("ioctl(PERF_EVENT_IOC_RESET/ENABLE)");
        close(fd);
        return 1;
    }

    volatile unsigned long x = 0;
    for (unsigned long i = 0; i < 10000000UL; i++)
        x += i;
    (void)x;

    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
        perror("ioctl(PERF_EVENT_IOC_DISABLE)");
        close(fd);
        return 1;
    }

    uint64_t count = 0;
    ssize_t n;
    do {
        n = read(fd, &count, sizeof count);
    } while (n < 0 && errno == EINTR);

    if (n != (ssize_t)sizeof count) {
        if (n < 0)
            perror("read(perf)");
        else
            fprintf(stderr, "read(perf): short read\n");
        close(fd);
        return 1;
    }

    printf("instructions_positive=%s count_over_1m=%s\n",
           count > 0 ? "yes" : "no", count > 1000000 ? "yes" : "no");

    close(fd);
    return 0;
}

/* ================= EXPLANATION =================
   What it does
     Opens a hardware instruction counter for the calling thread, enables
     it, runs about ten million volatile increments in user space,
     disables it, and reads the 64-bit count. It prints only booleans, so
     the result does not depend on compiler optimisation or CPU model.

   Step by step
     1. perf_event_attr is zeroed, then type=PERF_TYPE_HARDWARE,
        config=PERF_COUNT_HW_INSTRUCTIONS, size=sizeof(attr),
        disabled=1, and exclude_kernel=1 are set. size is the ABI version
        that tells the kernel which trailing fields are present.
     2. perf_event_open(attr, pid=0, cpu=-1, group_fd=-1, flags=0)
        attaches a per-task event to the calling thread. It counts on
        whichever CPU that thread runs on, across migrations.
     3. ioctl RESET zeroes the counter and ENABLE starts it. The volatile
        loop prevents the compiler from deleting the work.
     4. DISABLE freezes the count, then read() returns one uint64_t.
     5. The checks are count > 0 and count > 1,000,000. The exact number
        is deliberately not printed because it varies by build and CPU.

   Output
     instructions_positive=yes count_over_1m=yes

   Why it matters
     This is the PMU interface behind perf stat, sampling profilers, and
     low-overhead production monitoring. It needs no ptrace, no
     instrumentation, and no cooperation from the target thread.

   Caveats
     - perf_event_paranoid controls unprivileged access. EACCES/EPERM
       usually means the sysctl is too strict or the process lacks
       CAP_PERFMON/CAP_SYS_ADMIN; ENOENT often means no PMU is available
       (common in VMs). The program then prints one skipped line.
     - pid=0, cpu=-1 is per-thread. pid>0 follows that task; pid=-1,
       cpu>=0 is system-wide per-CPU and needs more privilege.
     - When more events are opened than there are counters, the kernel
       multiplexes them over time. Ask for PERF_FORMAT_TOTAL_TIME_ENABLED
       and _RUNNING if you need to detect or correct that.
     - Retired-instruction counts are not a time measure and vary with
       frequency, SMT sharing, and virtualisation.
     - exclude_kernel=1 keeps the count in user mode. read() works after
       DISABLE; disabling first gives a frozen value rather than a moving
       one. The event is per open file descriptor.
   ============================================== */
