# 100 Code Magicks

A collection of classic programming "magicks": small pieces of code that exploit a
language rule, memory layout, CPU guarantee, kernel interface, or probabilistic
property to do something surprising. Each entry is a standalone program followed by
an explanation of what it does, why it works, and where it breaks.

The tricks are not magic in the supernatural sense. Most are consequences of a
precise contract - object layout, pointer aliasing, memory ordering, control-flow
semantics, hash properties, or a kernel ABI. The point is to understand the contract,
not just copy the incantation.

## Running an entry

Each file is self-contained. From this directory:

```powershell
# C
gcc -std=gnu11 -O2 -Wall -Wextra -pthread 54_ticket_lock.c -o ticket.exe
.\ticket.exe

# C++
g++ -std=gnu++20 -O2 -Wall -Wextra 27_constexpr_string.cpp -o constexpr.exe
.\constexpr.exe

# Python
python 28_descriptor.py
```

On platforms where the math library is separate, add `-lm`. Threaded entries need
`-pthread` on GCC/Clang. The code is intentionally compact and teaching-oriented;
the **Caveats** section of each explanation lists the production concerns.

`backup/` is an earlier partial snapshot. The numbered files in the root are the
canonical 01-100 collection.

## Index

### 01-20: C foundations, bits, and ABI

| # | Entry | Trick |
|---:|---|---|
| 01 | [01_duffs_device.c](01_duffs_device.c) | A switch woven into a loop unrolls a byte copy. |
| 02 | [02_intrusive_list.c](02_intrusive_list.c) | Embed list links in the payload; recover the object with `container_of`. |
| 03 | [03_statement_expr_typeof.c](03_statement_expr_typeof.c) | A type-generic macro that evaluates each argument once. |
| 04 | [04_cleanup_defer.c](04_cleanup_defer.c) | A variable cleanup attribute gives C a small `defer`. |
| 05 | [05_tagged_pointer.c](05_tagged_pointer.c) | Alignment leaves spare pointer bits for tags. |
| 06 | [06_arena_allocator.c](06_arena_allocator.c) | Free many objects by resetting one bump pointer. |
| 07 | [07_branchless_minmax.c](07_branchless_minmax.c) | Select min/max with a sign mask instead of a branch. |
| 08 | [08_likely_unlikely.c](08_likely_unlikely.c) | Tell the compiler which branch is hot. |
| 09 | [09_xmacro_enum.c](09_xmacro_enum.c) | Replay one list under different macro definitions. |
| 10 | [10_flexible_array.c](10_flexible_array.c) | Header and variable payload share one allocation. |
| 11 | [11_ring_buffer.c](11_ring_buffer.c) | Modulo becomes a mask for power-of-two capacity. |
| 12 | [12_seqlock.c](12_seqlock.c) | Readers retry when a writer changes the version counter. |
| 13 | [13_spinlock_pause.c](13_spinlock_pause.c) | A pause hint reduces contention in a spin loop. |
| 14 | [14_cacheline_padding.c](14_cacheline_padding.c) | Pad counters to stop false sharing. |
| 15 | [15_endian_byteswap.c](15_endian_byteswap.c) | Reverse byte order with shifts, no temporary array. |
| 16 | [16_bitset_scan.c](16_bitset_scan.c) | Isolate the lowest set bit and map it to an index. |
| 17 | [17_fast_inverse_sqrt.c](17_fast_inverse_sqrt.c) | Float exponent bits seed a fast inverse-square-root guess. |
| 18 | [18_saturating_add.c](18_saturating_add.c) | Bound-check before addition to avoid signed overflow UB. |
| 19 | [19_zero_copy_iovec.c](19_zero_copy_iovec.c) | Describe one message as several buffers. |
| 20 | [20_syscall_wrapper.c](20_syscall_wrapper.c) | Cross the Linux x86-64 syscall boundary with registers. |

### 21-27: C++ type machinery

| # | Entry | Trick |
|---:|---|---|
| 21 | [21_manual_coroutine.cpp](21_manual_coroutine.cpp) | A coroutine as a switch over a saved program counter. |
| 22 | [22_small_buffer_erasure.cpp](22_small_buffer_erasure.cpp) | Erase a callable's type while keeping small callables inline. |
| 23 | [23_scope_guard.cpp](23_scope_guard.cpp) | Cleanup is an object whose destructor runs during unwinding. |
| 24 | [24_perfect_forwarding.cpp](24_perfect_forwarding.cpp) | Preserve lvalue/rvalue-ness through a wrapper. |
| 25 | [25_virtual_layout.cpp](25_virtual_layout.cpp) | Virtual dispatch is data: an object usually begins with a vptr. |
| 26 | [26_placement_new.cpp](26_placement_new.cpp) | Storage and construction are separate operations. |
| 27 | [27_constexpr_string.cpp](27_constexpr_string.cpp) | The compiler executes a parser while compiling. |

### 28-36: Python object model and runtime

| # | Entry | Trick |
|---:|---|---|
| 28 | [28_descriptor.py](28_descriptor.py) | An attribute is executable behavior, not just stored data. |
| 29 | [29_metaclass_registry.py](29_metaclass_registry.py) | Defining a class runs registration code immediately. |
| 30 | [30_generator_coroutine.py](30_generator_coroutine.py) | A generator is a suspended frame with locals and an instruction pointer. |
| 31 | [31_contextmanager.py](31_contextmanager.py) | A generator plus a decorator becomes structured cleanup. |
| 32 | [32_memoryview.py](32_memoryview.py) | `memoryview` exposes bytes without copying. |
| 33 | [33_slots.py](33_slots.py) | `__slots__` replaces a per-instance dict with fixed descriptors. |
| 34 | [34_import_hook.py](34_import_hook.py) | Importing is a protocol with pluggable finders and loaders. |
| 35 | [35_tracing.py](35_tracing.py) | CPython can call a Python function on execution events. |
| 36 | [36_missing_attribute.py](36_missing_attribute.py) | `__getattr__` runs only after ordinary lookup fails. |

### 37-48: Low-level systems and bit tricks

| # | Entry | Trick |
|---:|---|---|
| 37 | [37_restrict_vector.c](37_restrict_vector.c) | A no-alias promise unlocks vectorization. |
| 38 | [38_computed_goto.c](38_computed_goto.c) | A label-address table becomes a threaded-code interpreter. |
| 39 | [39_thread_local.c](39_thread_local.c) | Each thread gets its own copy of a variable. |
| 40 | [40_setjmp_error.c](40_setjmp_error.c) | Save and restore a stack context for non-local jumps. |
| 41 | [41_alloca_stack.c](41_alloca_stack.c) | Allocate a runtime-sized array on the stack. |
| 42 | [42_bit_reverse.c](42_bit_reverse.c) | Reverse 32 bits with five masked swaps. |
| 43 | [43_popcount_swar.c](43_popcount_swar.c) | Count set bits in parallel with SWAR arithmetic. |
| 44 | [44_debruijn_ctz.c](44_debruijn_ctz.c) | A De Bruijn sequence maps a low bit to its index. |
| 45 | [45_isqrt_digit.c](45_isqrt_digit.c) | Exact integer square root, one result bit at a time. |
| 46 | [46_float_ulp.c](46_float_ulp.c) | IEEE-754 bit patterns expose exponent, mantissa, and ULP. |
| 47 | [47_packed_wire.c](47_packed_wire.c) | Remove padding from a struct to match a wire layout. |
| 48 | [48_mmap_window.c](48_mmap_window.c) | Map a file into memory and let the page cache serve loads. |

### 49-59: Concurrency, atomics, and reclamation

| # | Entry | Trick |
|---:|---|---|
| 49 | [49_treiber_stack.c](49_treiber_stack.c) | A lock-free stack is one atomic head and a CAS loop. |
| 50 | [50_hazard_pointer.c](50_hazard_pointer.c) | Publish hazard pointers so reclaimers know what readers may touch. |
| 51 | [51_rcu_grace.c](51_rcu_grace.c) | RCU removes first, waits for readers, then frees. |
| 52 | [52_sharded_counter.c](52_sharded_counter.c) | Per-thread cache-line-padded counters avoid contention. |
| 53 | [53_sense_barrier.c](53_sense_barrier.c) | A generation bit makes a barrier reusable. |
| 54 | [54_ticket_lock.c](54_ticket_lock.c) | Two counters give a FIFO spinlock. |
| 55 | [55_dclp_atomic.c](55_dclp_atomic.c) | Double-checked locking with release/acquire publication. |
| 56 | [56_thread_pool.c](56_thread_pool.c) | Workers sleep on one condition variable and share a task queue. |
| 57 | [57_vyukov_queue.c](57_vyukov_queue.c) | Per-slot sequence numbers make a bounded MPMC queue. |
| 58 | [58_work_stealing.c](58_work_stealing.c) | Owner pops LIFO; thieves steal FIFO. |
| 59 | [59_message_passing.c](59_message_passing.c) | A release store and acquire load pass a message with no lock. |

### 60-72: Game-engine patterns

| # | Entry | Trick |
|---:|---|---|
| 60 | [60_ecs_sparse.c](60_ecs_sparse.c) | Sparse-set ECS storage: O(1) lookup, packed iteration. |
| 61 | [61_object_pool.c](61_object_pool.c) | Thread a free list through unused objects. |
| 62 | [62_double_buffer.c](62_double_buffer.c) | Render one frame while simulating the other, then swap. |
| 63 | [63_fixed_timestep.c](63_fixed_timestep.c) | Accumulate real time and consume fixed simulation steps. |
| 64 | [64_quat_slerp.c](64_quat_slerp.c) | Interpolate rotations on the quaternion sphere. |
| 65 | [65_spatial_hash.c](65_spatial_hash.c) | Hash grid cells to avoid all-pairs proximity tests. |
| 66 | [66_swept_aabb.c](66_swept_aabb.c) | Slab method finds the first time two moving boxes touch. |
| 67 | [67_bvh.c](67_bvh.c) | A bounding-volume hierarchy rejects whole subtrees. |
| 68 | [68_frustum_cull.c](68_frustum_cull.c) | Extract frustum planes from the projection matrix. |
| 69 | [69_lod_hysteresis.c](69_lod_hysteresis.c) | State-dependent thresholds stop LOD flicker. |
| 70 | [70_soa_simd.c](70_soa_simd.c) | Structure-of-arrays layout enables wide loads. |
| 71 | [71_fn_state.c](71_fn_state.c) | A function-pointer table replaces a switch-based FSM. |
| 72 | [72_command_buffer.c](72_command_buffer.c) | Record operations now, replay them later. |

### 73-82: Compilers, VMs, and runtimes

| # | Entry | Trick |
|---:|---|---|
| 73 | [73_inline_cache.py](73_inline_cache.py) | Cache the method for a receiver type at a call site. |
| 74 | [74_nan_boxing.c](74_nan_boxing.c) | Hide pointers and integers in a NaN payload. |
| 75 | [75_rope.py](75_rope.py) | Represent a string as a concatenation tree. |
| 76 | [76_mark_sweep.py](76_mark_sweep.py) | Reachability decides liveness; sweep the rest. |
| 77 | [77_refcount_weak.py](77_refcount_weak.py) | Weak references observe without owning; tracing collects cycles. |
| 78 | [78_ssa_fold.py](78_ssa_fold.py) | Rename definitions, then fold constants. |
| 79 | [79_linear_scan.py](79_linear_scan.py) | Allocate registers by scanning live intervals. |
| 80 | [80_trampoline.c](80_trampoline.c) | Run tail calls as returned thunks in one loop. |
| 81 | [81_bytecode_vm.py](81_bytecode_vm.py) | A stack machine executes one opcode at a time. |
| 82 | [82_hash_consing.py](82_hash_consing.py) | Intern immutable nodes by structure. |

### 83-92: OS, processes, and security

| # | Entry | Trick |
|---:|---|---|
| 83 | [83_spawn_process.c](83_spawn_process.c) | Create a child process with its own address space and exit status. |
| 84 | [84_pipe_pipeline.c](84_pipe_pipeline.c) | A pipe is a kernel byte buffer with EOF on writer close. |
| 85 | [85_shared_memory.c](85_shared_memory.c) | Map one physical page twice and write through either view. |
| 86 | [86_file_lock.c](86_file_lock.c) | A kernel byte-range lock, not a process-local flag. |
| 87 | [87_signal_handler.c](87_signal_handler.c) | Async interruption with only `sig_atomic_t` state. |
| 88 | [88_epoll_et.c](88_epoll_et.c) | Edge-triggered readiness must be drained to EAGAIN. |
| 89 | [89_sendfile_copy.c](89_sendfile_copy.c) | Move page-cache data without a user-space bounce buffer. |
| 90 | [90_seccomp_filter.c](90_seccomp_filter.c) | A BPF program filters syscalls before the kernel runs them. |
| 91 | [91_drop_privileges.c](91_drop_privileges.c) | Drop groups, gid, and uid so root cannot be regained. |
| 92 | [92_chroot_jail.c](92_chroot_jail.c) | Change the root of path resolution; it is not a full sandbox. |

### 93-100: Data structures and distributed systems

| # | Entry | Trick |
|---:|---|---|
| 93 | [93_bloom_filter.c](93_bloom_filter.c) | Probabilistic membership with no false negatives. |
| 94 | [94_crc32_table.c](94_crc32_table.c) | A lookup table turns polynomial division into byte operations. |
| 95 | [95_lru_cache.py](95_lru_cache.py) | Hash map plus linked list gives O(1) LRU. |
| 96 | [96_consistent_hash.py](96_consistent_hash.py) | A hash ring moves only a node's share when membership changes. |
| 97 | [97_vector_clock.py](97_vector_clock.py) | Component-wise counters recover causality without a global clock. |
| 98 | [98_raft_election.py](98_raft_election.py) | Terms and majority votes elect a leader and depose stale ones. |
| 99 | [99_skip_list.py](99_skip_list.py) | Random express lanes give expected O(log n) search. |
| 100 | [100_hyperloglog.py](100_hyperloglog.py) | Estimate distinct count from tiny leading-zero registers. |

## Verification

Verified in this workspace with GCC 16.1.0 (MinGW-w64, Windows) and Python 3.12.10:

- **77/77 C and C++ entries compile** with `-Wall -Wextra -Werror` and exit 0.
- **23/23 Python entries run** and exit 0.
- So all 100 entries build and run on the Windows host in at least their portable
  or stub form.

Platform notes:

- `20_syscall_wrapper.c` compiles the real inline-assembly path only on Linux
  x86-64; elsewhere it builds a stub.
- `88_epoll_et.c`, `89_sendfile_copy.c`, and `90_seccomp_filter.c` contain real
  Linux implementations and compile-time stubs for other platforms. The stubs
  were compiled and run here; the Linux bodies were reviewed but not executed
  on this Windows host.
- `91_drop_privileges.c` and `92_chroot_jail.c` are POSIX-only. Their Windows
  stubs run here; the privileged paths require a Unix-like host and root.
- `83`-`86` and `48` have both Windows and POSIX branches. The Windows branches
  were executed; the POSIX branches were not.
- Threaded entries have deterministic final results, but output interleaving can
  vary. `77_refcount_weak.py` depends on CPython's cycle collector, and
  `100_hyperloglog.py` is an estimate whose exact value depends on the hash and
  Python build.
