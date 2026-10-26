/* MAGIC: accumulate real elapsed time, run whole fixed-size simulation
   steps, and keep the remainder as the interpolation alpha. */
#include <stdio.h>
#define FIXED 0.25
int main(void)
{
    double frame_dt[] = {0.10, 0.20, 0.40, 0.30};
    double acc = 0.0;
    int tick = 0;
    for (int i = 0; i < 4; i++) {
        acc += frame_dt[i];
        int steps = 0;
        while (acc >= FIXED) {
            acc -= FIXED;
            tick++;
            steps++;
        }
        printf("frame %d dt=%.2f steps=%d tick=%d alpha=%.2f\n",
               i, frame_dt[i], steps, tick, acc / FIXED);
    }
}

/* ================= EXPLANATION =================
   What it does
     Separates simulation time from rendering time. The loop adds each
     variable frame duration to an accumulator, consumes it in fixed
     increments (0.25 s), and keeps the leftover fraction for render
     interpolation.

   Step by step
     1. Frame 0 lasts 0.10 s, less than one fixed step. No simulation tick
        runs; the accumulator holds 0.10, so alpha = 0.10/0.25 = 0.40.
     2. Frame 1 adds 0.20 for a total of 0.30. One fixed step runs, the
        accumulator drops to 0.05, tick becomes 1, alpha = 0.20.
     3. Frame 2 adds 0.40 for 0.45. One step runs, leaving 0.20, tick 2,
        alpha 0.80.
     4. Frame 3 adds 0.30 for 0.50. Two steps run, leaving 0.00, tick 4,
        alpha 0.00. The simulation advanced by exactly 1.00 s over four
        frames, independent of the frame times.
     5. A renderer can blend between the previous and current simulation
        state by alpha to hide the discrete tick boundaries.

   Output
     "frame 0 dt=0.10 steps=0 tick=0 alpha=0.40"
     "frame 1 dt=0.20 steps=1 tick=1 alpha=0.20"
     "frame 2 dt=0.40 steps=1 tick=2 alpha=0.80"
     "frame 3 dt=0.30 steps=2 tick=4 alpha=0.00"

   Why it matters
     Fixed-timestep simulation is deterministic: the same inputs and the
     same number of ticks produce the same result, independent of frame
     rate. It also makes physics stable, because a constraint solver or
     integrator sees a constant dt. The accumulator pattern is standard in
     game loops, replay systems, and networked lockstep.

   Caveats
     - The "spiral of death": if a frame takes longer than the time needed
       to simulate its catch-up steps, the next frame has even more work.
       Clamp the number of steps per frame and drop or slow time.
     - Floating-point accumulation drifts; use integer ticks or a
       double with a tolerance for long-running simulations.
     - Alpha is only valid when rendering can interpolate between states;
       gameplay logic must run on the fixed tick.
     - Very small fixed steps are expensive; very large ones miss
       collisions and make controls feel coarse.
     - Networked games often use a fixed tick plus prediction and
       reconciliation, not just this local loop.
   ============================================== */
