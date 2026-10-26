/* MAGIC: render from one buffer while the simulation writes the other;
   swap one index instead of copying a whole frame. */
#include <stdio.h>
struct frame { int tick; int value[4]; };
static struct frame frames[2];
static int front;
static void simulate(struct frame *f, int tick)
{
    f->tick = tick;
    for (int i = 0; i < 4; i++) f->value[i] = tick + i;
}
static void render(const struct frame *f)
{
    printf("frame %d:", f->tick);
    for (int i = 0; i < 4; i++) printf(" %d", f->value[i]);
    putchar('\n');
}
int main(void)
{
    front = 0;
    for (int tick = 0; tick < 3; tick++) {
        simulate(&frames[1 - front], tick);
        front = 1 - front;
        render(&frames[front]);
    }
}

/* ================= EXPLANATION =================
   What it does
     Keeps two complete frame buffers. The simulation writes into the back
     buffer while the renderer reads the front buffer. When the new frame
     is complete, the program swaps the two roles by flipping one index.

   Step by step
     1. front starts at 0, so frames[1] is the back buffer. simulate()
        writes all four values and the tick number into frames[1].
     2. `front = 1 - front` changes front to 1. render() now reads
        frames[1], which is complete. If the renderer were running on
        another thread, it would never observe a half-written frame.
     3. On the next tick, simulation writes frames[0] while the renderer
        can still read frames[1]. The swap happens only after the write.
     4. The loop prints frames 0, 1, and 2 with values 0..3, 1..4, and
        2..5. No pixel or value array was copied; only the index changed.
     5. The same pattern generalises to triple buffering: one front buffer
        being displayed and two back buffers, so the CPU does not have to
        wait for the display to finish with the previous frame.

   Output
     "frame 0: 0 1 2 3"
     "frame 1: 1 2 3 4"
     "frame 2: 2 3 4 5"

   Why it matters
     Double buffering prevents tearing and data races between a producer
     and a consumer that run at different rates. It is used for graphics
     swap chains, audio buffers, simulation snapshots, and database
     page writes. The swap is O(1), and the reader always sees a
     consistent version.

   Caveats
     - The swap itself must be atomic if producer and consumer are
       threads. A plain int index needs a mutex, atomic exchange, or a
       seqlock; otherwise the reader can see the new index with a
       partially written buffer.
     - Double buffering adds one frame of latency. Triple buffering
       reduces stalls but adds another.
     - The back buffer must not still be in use by the GPU/display when
       it is reused; real swap chains use fences or vsync.
     - Two buffers are not enough for arbitrary readers that hold a frame
       for a long time; use a pool or reference counting.
     - This demo has no threads; it shows the ownership pattern, not the
       synchronisation.
   ============================================== */
