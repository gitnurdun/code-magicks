/* ASM MAGICK #33 - JIT trampoline: build a function in memory and call it.
   Build: gcc -O2 -std=c11 33_jit_trampoline.c -o 33_jit_trampoline.exe
   MAGIC: code is data. Six bytes of machine code become a callable
   function after one VirtualProtect and one icache flush. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

int main(void)
{
    /* endbr64; lea eax,[rcx+rdx]; ret
       endbr64 keeps CET/IBT happy; LEA adds the two Windows x64 integer
       arguments; RET returns EAX to the caller. */
    static const unsigned char code[] = {
        0xF3, 0x0F, 0x1E, 0xFA,       /* endbr64              */
        0x8D, 0x04, 0x11,             /* lea eax, [rcx+rdx]   */
        0xC3                          /* ret                  */
    };

    void *mem = VirtualAlloc(NULL, sizeof code,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { printf("[33] VirtualAlloc failed\n"); return 1; }
    memcpy(mem, code, sizeof code);

    DWORD old = 0;
    if (!VirtualProtect(mem, sizeof code, PAGE_EXECUTE_READ, &old)) {
        printf("[33] VirtualProtect failed\n");
        VirtualFree(mem, 0, MEM_RELEASE);
        return 1;
    }
    FlushInstructionCache(GetCurrentProcess(), mem, sizeof code);

    int (*add)(int, int) = (int (*)(int, int))mem;
    printf("[33] JIT add(20, 22) = %d\n", add(20, 22));
    printf("[33] code bytes:");
    for (size_t i = 0; i < sizeof code; i++) printf(" %02x", code[i]);
    printf("\n");

    VirtualFree(mem, 0, MEM_RELEASE);
    return 0;
}

/* ================= EXPLANATION =================
   The machine code
     F3 0F 1E FA   ENDBR64: a no-op on CPUs without CET, and the required
                   indirect-branch target marker when IBT is enabled.
     8D 04 11      LEA EAX, [RCX+RDX]: compute the sum of the two args
                   without touching flags. (LEA is an address calculation,
                   not a load.)
     C3            RET: return with EAX as the integer result.

   The lifecycle
     1. VirtualAlloc reserves and commits pages with PAGE_READWRITE. You
        can write to them, but executing them would fault (DEP).
     2. memcpy copies the bytes.
     3. VirtualProtect changes the protection to PAGE_EXECUTE_READ. This is
        the W^X discipline: never leave a page writable and executable at
        the same time if you can avoid it.
     4. FlushInstructionCache makes sure the CPU's instruction fetch sees
        the new bytes. On x86, coherent icache usually makes this
        unnecessary, but the API is required for correctness on other
        architectures and for the Windows memory manager's bookkeeping.
     5. Cast the pointer to the correct function type and call it.

   Why JITs do this
     - Copy-and-patch JITs (used in regex engines, database query
       compilers, and some language runtimes) emit small stencils.
     - Inline caches and polymorphic call sites rewrite a mov/jmp.
     - Emulators translate guest blocks into host code.
     - Games used to JIT shader/script code; modern consoles and iOS
       forbid it, which is why interpreters and AOT compilers matter.

   Output
     [33] JIT add(20, 22) = 42
     [33] code bytes: f3 0f 1e fa 8d 04 11 c3

   Gotchas
     - DEP/PaX/W^X: some systems forbid executable heap; iOS and many
       consoles do not allow JIT at all.
     - CET/CFG: if Control Flow Guard or IBT is enforced, the target must
       be a valid indirect-branch target (CFG call target or ENDBR64).
     - The ABI is platform-specific: RCX/RDX here is Windows x64; SysV
       would use RDI/RSI.
     - The code page must outlive the function pointer. Freeing it while a
       call is in flight is a use-after-free.
     - Self-modifying code needs care: stop other threads, flush, and
       respect the ISA's icache coherence rules.
     - Do not execute attacker-controlled bytes; this is a sandbox escape
       by design.
   ============================================== */
