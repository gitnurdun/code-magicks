/* MAGIC: cross the Linux x86-64 kernel boundary by placing values in ABI registers.
   CAVEAT: platform-specific teaching code, not a portable syscall library. */
#if defined(__linux__) && defined(__x86_64__)
#include <unistd.h>
#include <sys/syscall.h>
static long raw_write(int fd,const void*p,unsigned long n){long r;asm volatile("syscall":"=a"(r):"a"(SYS_write),"D"(fd),"S"(p),"d"(n):"rcx","r11","memory");return r;}
int main(void){const char s[]="hello via syscall\n";raw_write(1,s,sizeof s-1);}
#else
#include <stdio.h>
int main(void){puts("raw syscall demo is Linux x86-64 only");}
#endif

/* ================= EXPLANATION =================
   What it does
     Invokes the Linux x86-64 write(2) system call directly with inline
     assembly, bypassing the C library wrapper.

   Step by step
     1. The Linux x86-64 syscall ABI passes the call number in rax and the
        arguments in rdi, rsi, rdx, r10, r8, r9; the `syscall` instruction
        enters the kernel.
     2. The output operand "=a"(r) reserves rax for the return value; the
        input "a"(SYS_write) loads the write syscall number (1) into rax.
     3. "D"(fd) puts the file descriptor in rdi, "S"(p) the buffer in rsi,
        and "d"(n) the byte count in rdx - matching the write(fd, buf,
        count) signature.
     4. The clobber list says rcx and r11 are destroyed by the syscall
        (the CPU saves the return address and flags there), and "memory"
        tells the compiler that memory may be read or written, preventing
        it from reordering the buffer contents across the asm. `volatile`
        keeps the asm from being deleted or moved.
     5. raw_write returns the raw result from rax: a non-negative byte
        count on success, or a negative errno value on failure. main()
        writes 18 bytes of the string to fd 1 (stdout), excluding the
        trailing NUL.

   Output
     "hello via syscall"
     (on non-Linux-x86-64 platforms the stub prints
      "raw syscall demo is Linux x86-64 only")

   Why it matters
     It shows the exact boundary between user space and the kernel: the
     C library's write() is essentially this instruction plus errno
     handling, cancellation checks, and signal restarts. Understanding the
     ABI is necessary for syscall wrappers, sandboxes, and runtime
     libraries.

   Caveats
     - Linux x86-64 only. The register names, syscall numbers, and ABI are
       platform-specific; this file builds a stub on other platforms rather
       than attempting the raw syscall.
     - The return value is -errno on failure; this wrapper does not set
       errno, retry EINTR, or handle partial writes.
     - Prefer the libc syscall() wrapper or a proper library; raw inline
       asm is hard to audit and can conflict with sanitizers, tracing,
       seccomp filters, and the vDSO.
     - `asm` is not a C11 keyword; use __asm__ under -std=c11.
   ============================================== */
