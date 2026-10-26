/* MAGIC: switch LOD thresholds depend on the current level, creating a
   dead band that stops meshes from flickering at a boundary. */
#include <stdio.h>
static int lod_for(float distance, int current)
{
    if (current == 0 && distance > 10.0f) return 1;
    if (current == 1 && distance > 25.0f) return 2;
    if (current == 2 && distance < 20.0f) return 1;
    if (current == 1 && distance < 8.0f) return 0;
    return current;
}
int main(void)
{
    float distances[] = {5, 9, 11, 24, 26, 21, 19, 9, 7};
    int lod = 0;
    for (int i = 0; i < 9; i++) {
        lod = lod_for(distances[i], lod);
        printf("dist=%.0f lod=%d\n", distances[i], lod);
    }
}

/* ================= EXPLANATION =================
   What it does
     Chooses one of three levels of detail (LOD) from distance, but uses
     different thresholds to go up and down. The current level is part of
     the decision, so a mesh hovering near a threshold does not switch
     back and forth every frame.

   Step by step
     1. LOD 0 is the highest-detail mesh, LOD 1 is medium, LOD 2 is low.
        Going up: 0 -> 1 above distance 10, 1 -> 2 above 25.
     2. Going down: 2 -> 1 below 20, 1 -> 0 below 8. The gaps 8..10 and
        20..25 are hysteresis bands: once a switch happens, the object
        must move well past the old threshold before switching back.
     3. At 5 and 9 the object stays at LOD 0. At 11 it promotes to 1; at
        24 it stays; at 26 it promotes to 2.
     4. Moving closer: 21 still stays at 2, 19 demotes to 1 (below 20).
        At 9 it stays at 1 (not below 8), and at 7 it demotes to 0.
     5. The sequence shows no repeated toggling at 9 or 21, which a single
        threshold would produce.

   Output
     "dist=5 lod=0"
     "dist=9 lod=0"
     "dist=11 lod=1"
     "dist=24 lod=1"
     "dist=26 lod=2"
     "dist=21 lod=2"
     "dist=19 lod=1"
     "dist=9 lod=1"
     "dist=7 lod=0"

   Why it matters
     LOD reduces vertex, texture, and draw cost for distant objects, but
     naive threshold switching causes visible popping and wasted work when
     the camera jitters around a boundary. Hysteresis makes the decision
     stable. The same state-dependent threshold idea appears in physics
     sleeping, audio streaming, AI perception, and network interest
     management.

   Caveats
     - Hysteresis trades responsiveness for stability; the band must be
       wider than camera jitter but not so wide that quality lags.
     - A single distance is a crude metric. Screen-space error, projected
       size, motion, and importance give better results.
     - Promotion and demotion usually have different costs; changing
       meshes can cause a hitch, so preload and cross-fade.
     - The thresholds are hard-coded here; real systems scale them by
       FOV, resolution, and object size.
     - LOD transitions can still pop without blending, dithering, or
       alpha fade.
   ============================================== */
