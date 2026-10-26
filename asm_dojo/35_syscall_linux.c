/* ASM MAGICK #35 - raw Linux syscall: the kernel ABI in six registers.
   Build: gcc -O2 -std=c11 35_syscall_linux.c -o 35_syscall_linux.exe
   MAGIC: `syscall` is the user/kernel boundary; libc's write() is this
   instruction plus errno handling and cancellation checks. */
#include <stdio.h>

#if defined(__linux__) && defined(__x86_64__)
#include <unistd.h>
#include <sys/syscall.h>

static long raw_write(int fd, const void *p, unsigned long n)
{
    long r;
    /* rax = syscall number, rdi/rsi/rdx = args, syscall clobbers rcx/r11. */
    __asm__ volatile("syscall"
                     : "=a"(r)
                     : "a"((long)SYS_write), "D"(fd), "S"(p), "d"(n)
                     : "rcx", "r11", "memory");
    return r;
}

int main(void)
{
    const char s[] = "hello via raw syscall\n";
    long n = raw_write(1, s, sizeof s - 1);
    if (n != (long)(sizeof s - 1))
        perror("raw_write");
    return 0;
}
#else
int main(void)
{
    printf("[35] raw syscall demo is Linux x86-64 only; this host is not.\n");
    printf("[35] the instruction sequence and ABI are still explained below.\n");
    return 0;
}
#endif

/* ================= EXPLANATION =================
   The Linux x86-64 syscall ABI
     rax  syscall number (write = 1)
     rdi  arg0 (fd)
     rsi  arg1 (buffer)
     rdx  arg2 (count)
     r10  arg3
     r8   arg4
     r9   arg5
     The `syscall` instruction enters the kernel at LSTAR, saves the return
     RIP in rcx and RFLAGS in r11, and returns the result in rax. It is not
     a function call: there is no return address on the stack, and rcx/r11
     are destroyed.

   Reading the asm
     "=a"(r) reserves rax for the result; "a"(SYS_write) loads the number
     into rax; "D"/"S"/"d" put fd, buffer and count into rdi/rsi/rdx. The
     clobber list says rcx and r11 are destroyed, and "memory" says the
     buffer may be read, so the compiler cannot reorder stores into it
     across the instruction. `volatile` keeps the asm from being deleted
     or duplicated.

   Return convention
     A non-negative value is success (bytes written). A negative value in
     [-4095, -1] is -errno (e.g., -EFAULT, -EINTR). libc's write() turns
     that into -1 and sets errno, retries EINTR in some cases, and handles
     cancellation. The raw wrapper does none of that.

   Why it matters
     - Syscall wrappers, sandboxes and seccomp filters (#90) need the
       exact ABI.
     - vDSO functions (gettimeofday, clock_gettime) are user-space code
       the kernel maps in, avoiding the syscall cost.
     - strace/ptrace, seccomp, and syscall-interposition tools work at
       this boundary.
     - The same pattern appears on other architectures with different
       registers and numbers; it is not portable.

   Output
     On Linux x86-64:
       hello via raw syscall
     On this Windows host:
       [35] raw syscall demo is Linux x86-64 only; this host is not.
       [35] the instruction sequence and ABI are still explained below.

   Gotchas
     - Linux x86-64 only. The instruction, register numbers and syscall
       numbers differ on aarch64, x86-32 (int 0x80) and Windows.
     - The raw call does not retry EINTR, handle partial writes, or set
       errno.
     - Sanitizers, seccomp, and tracing may intercept or forbid syscalls.
     - `asm` is not a C11 keyword; use `__asm__` under -std=c11.
     - Do not use this in a library; use the libc wrapper.
   ============================================== */
