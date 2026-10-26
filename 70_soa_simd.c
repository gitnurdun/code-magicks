/* MAGIC: store each component in its own contiguous array. The loop then
   has unit-stride loads that a compiler can turn into SIMD. */
#include <stdio.h>
#define N 1024
static float px[N], py[N];
static float vx[N], vy[N];
int main(void)
{
    for (int i = 0; i < N; i++) {
        px[i] = (float)i;
        py[i] = (float)(2 * i);
        vx[i] = 1.0f;
        vy[i] = 2.0f;
    }
    const float dt = 0.5f;
    for (int step = 0; step < 10; step++)
        for (int i = 0; i < N; i++) {
            px[i] += vx[i] * dt;
            py[i] += vy[i] * dt;
        }
    printf("px[0]=%.0f px[3]=%.0f py[3]=%.0f\n", px[0], px[3], py[3]);
}

/* ================= EXPLANATION =================
   What it does
     Integrates 1024 particles for ten fixed steps, storing x, y, vx, and
     vy in four separate arrays instead of one array of structs. This is
     the Structure of Arrays (SoA) layout.

   Step by step
     1. Initialisation fills px = 0..1023, py = 0,2,4,..., vx = 1, and
        vy = 2. Each array is contiguous, so px[i] and px[i+1] are next to
        each other in memory.
     2. The inner loop updates all x positions, then all y positions.
        At each statement the compiler sees unit-stride loads from two
        arrays and a unit-stride store to one array. With -O3 and a SIMD
        target it can process 4, 8, or 16 floats per instruction after
        checking alignment and aliasing.
     3. The same loop with an Array of Structures (AoS) would interleave
        x, y, vx, vy per particle. Updating only x still pulls in whole
        cache lines containing y and velocities, and the SIMD lanes would
        be split across unrelated fields.
     4. After 10 steps at dt = 0.5, x advances by 5 and y by 10. Thus
        px[0] = 5, px[3] = 3 + 5 = 8, and py[3] = 6 + 10 = 16.
     5. The computation is identical to AoS; only the memory layout and
        therefore the achievable vectorisation differ.

   Output
     "px[0]=5 px[3]=8 py[3]=16"

   Why it matters
     SoA is the default for particle systems, physics broadphase, GPU
     buffers, and ECS component storage. It maximises useful bytes per
     cache line and lets the compiler or GPU coalesce accesses. Data-
     oriented design often beats clever scalar code simply by making the
     hardware's wide loads useful.

   Caveats
     - SoA is not always faster. If a system needs all fields of one
       object together, AoS has better locality and fewer streams.
     - The compiler may not vectorise without alignment, `restrict`, or
       an appropriate -march; inspect the assembly or use explicit SIMD.
     - Four arrays mean four base pointers and more TLB/cache streams;
       huge N can thrash.
     - GPU APIs often require a specific layout (AoSoA or packed vertex
       buffers); SoA is not universal.
     - Adding a new component means adding another array and updating
       every system that iterates it.
   ============================================== */
