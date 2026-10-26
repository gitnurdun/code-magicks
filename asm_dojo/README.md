# ASM MAGICK — 35 runnable instruction-level demos

A companion to the 100-snippet `code-magicks` collection: the same kind of
short, dense, "how did anyone think of that" code, but focused on what the
machine actually executes.

Every demo is a self-contained `.c` file with a `MAGIC` line, a build line,
the code, and an `EXPLANATION` block (what it does, step by step, output, why
it matters, caveats). This README is the map and the cross-reference.

---

## 1. The honest inventory

I read the collection before writing any demos. Of the numbered snippets:

- **1 contains literal inline assembly** — `20_syscall_wrapper.c`, the Linux
  x86-64 `syscall` wrapper.
- **1 uses an x86 intrinsic** — `13_spinlock_pause.c`, `_mm_pause()`.
- **About 20 are assembly-adjacent** — they are magic because of what the
  compiler must emit: bit-punned floats, computed goto, alloca, setjmp,
  cmpxchg-based lock-free structures, SIMD-friendly layout, and so on.
- **The rest are algorithmic or language-level tricks** (Duff's device, arena
  allocator, Python descriptors, Raft election) with no instruction-level
  payload.

So there are not 30 asm snippets in the collection to catalogue. Instead of
padding a list, this directory is a **dojo**: 35 small demos that rebuild the
instruction-level ideas behind the collection, plus the kernel/game-engine
tricks those files point at. Each one runs, and each one is explained.

### What was actually underneath the originals

| Original | What the machine sees | Dojo counterpart |
|---|---|---|
| `20_syscall_wrapper.c` | `syscall` with rax/rdi/rsi/rdx; clobbers rcx/r11 | **35** |
| `13_spinlock_pause.c` | `pause` in the retry loop | **06** |
| `17_fast_inverse_sqrt.c` | `movd`/`shr`/`sub`/`mulss` on float bits; a constant input folds to a literal | **14** (bit-level view) |
| `38_computed_goto.c` | indirect `jmp [table+idx*8]`; constant bytecode can be folded away | **31** |
| `40_setjmp_error.c` | save/restore rip, rsp, callee-saved regs | **30** |
| `41_alloca_stack.c` | `sub rsp, N` / `add rsp, N` | **29** |
| `42_bit_reverse.c` | SWAR: five masked swaps (~15 ALU ops); `bswap` only reverses bytes | **14** (contrast), **12** (bit loop) |
| `43_popcount_swar.c` | SWAR vs one `popcnt` | **13** |
| `44_debruijn_ctz.c` | `imul` by 0x077CB531, `shr 27`, table load | **12** |
| `46_float_ulp.c` | integer increment on the float bit pattern | **14** |
| `70_soa_simd.c` | `movups`/`mulps`/`addps` when vectorized | **19–21** |
| `12_seqlock.c`, `53_sense_barrier.c`, `55_dclp_atomic.c` | atomic loads/stores plus fences | **25** |
| `49_treiber_stack.c` | `lock cmpxchg` retry loop | **07** |
| `54_ticket_lock.c` | `lock xadd` for the ticket | **08** |
| `57_vyukov_queue.c` | `lock xadd` + `lock cmpxchg` | **07**, **08** |
| `52_sharded_counter.c` | one locked add per shard; false sharing is the enemy | **08**, **09** |
| `58_work_stealing.c` | atomics on deque ends | **08** |
| `05_tagged_pointer.c` | mask the low alignment bits | **10**, **11** |
| `74_nan_boxing.c` | NaN payload bits distinguish pointers/ints | **14** |
| `47_packed_wire.c`, `15_endian_byteswap.c` | `bswap` / `movbe` | **14** |
| `90_seccomp_filter.c` | `syscall` plus a BPF filter | **35** |
| `21_manual_coroutine.cpp` | a state machine, not a real stack switch | **31** |
| `80_trampoline.c` | indirect call/return, no C-stack growth | **31**, **34** |

The takeaway: a "magic" C snippet is often magic because of one instruction
the compiler is allowed to use, or one instruction the programmer had to
write because C cannot express the operation.

---

## 2. How to read the assembly in this folder

### AT&T vs Intel

GCC's default output is AT&T syntax. The demos write AT&T, and the
explanations show the Intel equivalent. The two differ in operand order:

| AT&T (GCC default) | Intel (objdump `-Mintel`) | Meaning |
|---|---|---|
| `movl %eax, %ebx` | `mov ebx, eax` | `ebx = eax` |
| `addq $8, %rsp` | `add rsp, 8` | `rsp += 8` |
| `movl 8(%rbp), %eax` | `mov eax, [rbp+8]` | load a local |
| `leaq (%rax,%rcx,8), %rdx` | `lea rdx, [rax+rcx*8]` | address arithmetic |
| `cmpl %edx, %ecx` | `cmp ecx, edx` | compute `ecx - edx` |

- `%rax` is a register, `$42` is an immediate, `(%rax)` is a memory operand.
- The suffix is the width: `b` byte, `w` word, `l` long (32-bit), `q` quad
  (64-bit). `movl` moves 32 bits; `movq` moves 64.
- Intel order is `dst, src`; AT&T is `src, dst`. This is the single most
  common source of "the code looks right but adds the wrong things".

### Extended asm vs basic asm

```c
/* Extended asm: has operands, uses %0..%9 and %% for literal registers. */
__asm__ volatile("addl %1, %0" : "+r"(x) : "r"(y) : "cc");

/* Basic asm: no operands (common in naked functions), no % substitution.
   A single % is correct here; %% would reach the assembler literally. */
__asm__ volatile("movl $42, %eax\n\tret");
```

That distinction bit me while building demo 32: `%%eax` inside a naked
function's basic asm reaches GNU as as a bad register name. In extended asm,
`%%eax` is required. The rule is: **if the asm has a colon (an operand list),
double the percent; if it is a bare template with no colon, do not.**

### Constraint cheat sheet

| Constraint | Meaning |
|---|---|
| `"r"` | any general-purpose register |
| `"m"` | memory operand |
| `"i"` | immediate constant |
| `"a"`, `"d"`, `"D"`, `"S"` | rax/eax, rdx, rdi, rsi specifically |
| `"=r"(x)` | write-only output |
| `"+r"(x)` | read-write operand |
| `"&"` | early-clobber: output must not overlap an input |
| `"0"(x)` | input must use the same location as output 0 |
| `"q"` | a byte-addressable register (al, bl, ...) |

Clobbers that matter:

- `"cc"` — the asm changes EFLAGS.
- `"memory"` — the asm reads/writes memory; prevents compiler reordering.
- `"rcx", "r11"` — destroyed by the `syscall` instruction.
- `"xmm0"` — a vector register the template uses without naming as an operand.

`volatile` stops the compiler from deleting or duplicating the asm. It does
**not** make it a memory barrier, and it does **not** make it atomic.

### The ABI in one page

**Windows x64** (all runnable demos here, except 35):

- Integer args: `rcx`, `rdx`, `r8`, `r9`; then the stack.
- Return: `rax`/`eax`; floats in `xmm0`.
- Caller reserves 32 bytes of shadow space, and `rsp` must be 16-byte aligned
  before a `call`.
- Callee-saved: `rbx`, `rbp`, `rdi`, `rsi`, `r12`–`r15`, `xmm6`–`xmm15`.
- No red zone: do not write below `rsp` without adjusting it.

**System V x86-64** (Linux/macOS):

- Integer args: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`; then the stack.
- No shadow space; a 128-byte red zone below `rsp` is available to leaf
  functions.
- Callee-saved: `rbx`, `rbp`, `r12`–`r15`.

**Linux syscall** (demo 35):

- Number in `rax`; args in `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`.
- `syscall` clobbers `rcx` (return RIP) and `r11` (RFLAGS).
- Negative return in `[-4095, -1]` is `-errno`.

The same naked body is not portable between Windows and SysV. Demo 34 shows
the Windows contract explicitly; a SysV version would use different registers.

---

## 3. The 35 demos

### A. The contract (01–02)

The two rules that make inline asm safe: tell the compiler exactly what the
instruction reads and writes, and never let an output overwrite an input that
has not been consumed yet.

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 01 | `01_add_constraints.c` | `add`, `lea` | 42, 42, 42 |
| 02 | `02_early_clobber.c` | `xchg`, one-operand `imul` | 12, swapped values, 128-bit product verified |

**The trick:** `"=r"`, `"0"` and `"+r"` are a vocabulary for the register
allocator. `"&"` is the one-character fix for "my output is written before my
input is read". Fixed-register instructions (`imul`, `cpuid`, `rdtsc`,
`cmpxchg`, `syscall`) have no choice: you must state their registers.

### B. Asking the CPU (03–05)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 03 | `03_rdtsc.c` | `lfence`, `rdtsc` | ~1.0 cycles per volatile loop, ~0.4 per nop |
| 04 | `04_cpuid.c` | `cpuid` | GenuineIntel, family 6 model 154, SSE4.2/BMI2/RDRAND present |
| 05 | `05_xgetbv.c` | `xgetbv` | OSXSAVE=1, XCR0=0x7, AVX usable |

**The trick:** `rdtsc` is a 64-bit counter split across `edx:eax` and is not
serializing; `lfence` makes the measurement honest. `cpuid` returns four
answers and clobbers `rbx` (a callee-saved register), which is why `"=b"` is
not optional. `xgetbv` is the OS permission slip: the CPU can support AVX while
the kernel has not enabled the state, and executing AVX then faults.

### C. Waiting and sharing (06–09)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 06 | `06_pause.c` | `pause` | ~100–200 cycles per pause vs ~0.3 per nop |
| 07 | `07_cmpxchg_spinlock.c` | `lock cmpxchg` | acquire/release, success and failure semantics |
| 08 | `08_xadd.c` | `lock xadd` | returned 0, 10, 20; counter 10, 20, 30 |
| 09 | `09_lock_cost.c` | `inc` vs `lock inc` | ~0.6–1.8 vs ~11–30 cycles/op, ~7–18x |

**The trick:** `cmpxchg` compares `eax` with memory and conditionally stores;
`xadd` returns the old value while adding. Both are full barriers with the
`lock` prefix. The expensive part is not the arithmetic; it is taking
exclusive ownership of the cache line. `pause` is not a no-op: it
de-pipelines the retry loop and reduces coherence traffic.

### D. Branchless and bit-level (10–18)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 10 | `10_cmov.c` | `cmp`, `cmovge`, `cmovle` | min/max of 16 values, matches C |
| 11 | `11_setcc_abs.c` | `setl`, `cltd`, `xor`, `sub` | 0/1 comparison, abs, INT_MIN caveat |
| 12 | `12_bitscan.c` | `bsf`, `bsr`, `tzcnt`, `lzcnt` | bsf=20, bsr=23, tzcnt=20, lzcnt=8 |
| 13 | `13_popcnt.c` | `popcntq` vs SWAR | 32 bits, 0 mismatches; ~1.1–1.7 vs ~2.2–2.3 cycles/op |
| 14 | `14_bswap.c` | `bswap`, `movbe` | 0x11223344 → 0x44332211; wire bytes → 0x11223344 |
| 15 | `15_crc32.c` | `crc32b` | CRC32C("123456789") = 0xe3069283, matches software |
| 16 | `16_pdep_pext.c` | `pdepq`, `pextq` | pdep(0xab, 0xf0f)=0xa0b; round trip 0xab |
| 17 | `17_bmi2_shifts.c` | `shlx`, `shrx`, `sarx` | variable shifts without CL |
| 18 | `18_mulx.c` | `mulxq` | 64×64→128 product, agrees with `__int128` |

**The trick:** branchless code trades wasted work for avoided mispredictions.
`cmov` selects a value, not an instruction pointer; `setcc` materializes a
condition as a byte; the sign-mask `(x ^ mask) - mask` is a branchless abs.
Bit scans turn a bitset into indices. `popcnt`/`crc32`/`pdep`/`pext` are
single-instruction answers to problems that otherwise need a loop or a table.
The catch is always the same: check CPUID, check operand order, and benchmark
on the target (PDEP/PEXT are notoriously slow on Zen 1/2).

### E. SIMD (19–22)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 19 | `19_sse_paddd.c` | `movdqu`, `paddd` | {11,22,33,44} |
| 20 | `20_pshufb.c` | `pshufb` | 16 bytes reversed in one instruction |
| 21 | `21_pmaddwd.c` | `pmaddwd` | lanes {50,250,610,1130}; dot product 2040 |
| 22 | `22_pcmpeqb.c` | `pcmpeqb`, `pmovmskb`, `ctz` | first mismatch at index 6 |

**The trick:** SIMD is a wider register file, not a different kind of
computing. `paddd` does four independent adds; `pmaddwd` does eight multiplies
and four adds; `pcmpeqb` + `pmovmskb` turns 16 byte comparisons into a 16-bit
mask; `pshufb` treats a mask register as a 16-entry permutation table. The
hard parts are alignment, lane order, and reading past the end of a buffer.

### F. The memory hierarchy (23–25)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 23 | `23_prefetch.c` | `prefetcht0` | ~1.0–1.05x here; hardware prefetcher already helps |
| 24 | `24_clflush.c` | `clflush`, `mfence` | cached ~30–90 vs flushed ~200–550 cycles |
| 25 | `25_fences.c` | `mfence`, `lfence`, `sfence` | ~25 / ~8 / ~3.5 cycles/op |

**The trick:** a cache miss costs hundreds of cycles; prefetch and cache-line
discipline turn that latency into overlap. `clflush` makes the miss visible on
demand (and is how Flush+Reload works). Fences order visibility, not data:
`lfence` orders loads, `sfence` orders stores, `mfence` does both. x86's TSO
model only reorders store→load, which is why `mfence` is the one people
actually need.

### G. Machine state and control flow (26–35)

| # | File | Key instructions | Verified result |
|---|---|---|---|
| 26 | `26_flags_adc.c` | `cmp`, `pushfq`/`popq`, `adc` | CF/ZF/SF/OF decoded; 128-bit add = 1:0 |
| 27 | `27_rdrand.c` | `rdrand`, `rdseed`, `setc` | ok=1, random 64-bit values |
| 28 | `28_stack_frame.c` | `rsp`, `rbp`, return address | local at rsp+44, rbp-rsp=48 |
| 29 | `29_alloca_rsp.c` | `sub rsp`, `add rsp` | 64-byte scratch, value read back |
| 30 | `30_setjmp.c` | `setjmp`/`longjmp` | longjmp returned 7, marker preserved |
| 31 | `31_computed_goto.c` | indirect `jmp` through label table | 42 |
| 32 | `32_naked.c` | `mov eax,42`, `ret`, `cmov` | 42; max(17,42)=42 |
| 33 | `33_jit_trampoline.c` | `VirtualAlloc`, `VirtualProtect`, `lea`, `ret` | JIT add(20,22)=42 |
| 34 | `34_abi_thunk.c` | `sub rsp,40`, `call`, shadow space | 10 direct and via thunk |
| 35 | `35_syscall_linux.c` | `syscall` | Linux-only; prints a stub on Windows |

**The trick:** control flow is data. `setjmp` saves rip/rsp and the
callee-saved registers; `longjmp` reloads them and abandons the frames in
between without unwinding. Computed goto is a table of code addresses.
A naked function is exactly the bytes you wrote, with no prologue and no
unwind info. A JIT writes bytes, changes the page protection, flushes the
icache, and calls the result. The ABI thunk shows the contract that makes all
of it interoperable: registers, shadow space, alignment, callee-saved set.

---

## 4. Build and run

Windows, MinGW-w64 GCC (what this was verified with):

```powershell
cd asm_dojo
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_and_run.ps1
```

The explicit `-ExecutionPolicy Bypass` is only for that process; it is needed
on machines where running local `.ps1` files is disabled. If your policy
already allows local scripts, `.\build_and_run.ps1` works too.

Or one demo at a time:

```powershell
gcc -O2 -std=c11 -Wall -Wextra 07_cmpxchg_spinlock.c -o 07.exe
.\07.exe
```

Linux:

```bash
gcc -O2 -std=c11 -Wall -Wextra 07_cmpxchg_spinlock.c -o 07 && ./07
```

Most demos are portable x86-64. The exceptions:

- **33** uses `windows.h` / `VirtualAlloc` — Windows only.
- **34** hard-codes the Windows x64 ABI — adapt the registers for SysV.
- **32** is written against the Windows ABI — same caveat.
- **29** relies on there being no red zone (true on Windows; on SysV, a
  signal or interrupt could clobber the scratch area under `rsp`).
- **28** prints frame layout, which differs by compiler and flags.
- **35** is Linux-only, with a non-Linux stub so it still builds everywhere.

No external libraries. `-O2` matters: at `-O0` the compiler's own frame and
spills change what several of the stack/ABI demos print.

### What is deliberately not here

- Privileged instructions: `rdmsr`/`wrmsr`, `in`/`out`, `hlt`, `cli`/`sti`,
  `invlpg`, `mov cr3`, `swapgs`. They fault in user mode; a kernel module
  would need a different setup.
- Real kernel code: no GDT/IDT setup, no interrupt entry stubs, no context
  switch, no page tables. Demo 35 stops at the syscall boundary.
- Multi-threaded contention: the atomic demos are single-threaded so the
  output is deterministic. They teach the instruction semantics; a contention
  benchmark needs threads and a pinning strategy.
- AVX-512, TSX, CET shadow-stack programming, SGX. The CPU here does not
  expose AVX-512 state (XCR0=0x7), and the others need OS/runtime setup.

---

## 5. Verification

All 35 files were compiled and run on:

- Windows 11, x86-64 (Alder Lake, family 6 model 154)
- MinGW-w64 GCC 16.1.0, `-O2 -std=c11 -Wall -Wextra`
- 34 execute natively; **35** is a Linux-only demo and prints its stub here.

The captured run is in `VERIFIED_RUN.txt`. Timing numbers are TSC-based and
machine-specific; the explanations quote one representative run and say so.
Feature-dependent demos check CPUID first and print a "not available" path
instead of executing an illegal instruction.

To inspect the generated code yourself:

```powershell
gcc -O2 -S -masm=intel 01_add_constraints.c -o -
objdump -d -Mintel 01_add_constraints.exe
```

---

## 6. Suggested paths

- **Kernel / concurrency:** 06 → 07 → 08 → 09 → 25 → 35, then read the
  originals `12_seqlock.c`, `49_treiber_stack.c`, `51_rcu_grace.c`,
  `54_ticket_lock.c`, `57_vyukov_queue.c`.
- **Game engine / math:** 10 → 12 → 13 → 19 → 20 → 21 → 23 → 24, then
  `17_fast_inverse_sqrt.c`, `42_bit_reverse.c`, `70_soa_simd.c`.
- **"How is that even allowed?"** 16 → 17 → 18 → 31 → 32 → 33 → 34.
- **Measurement and CPU identification:** 03 → 04 → 05 → 06 → 09 → 13 →
  23 → 24 → 25. Always run timing demos more than once.

If you want to go deeper after this: rewrite one demo as a standalone NASM
`.asm` file, link it with `gcc`, and compare the disassembly. The instruction
is the easy part; the ABI, the clobbers, and the memory model are the craft.
