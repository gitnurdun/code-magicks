/* MAGIC: sweep two moving AABBs with the slab method to find the first
   time they touch, instead of testing only their current positions. */
#include <stdio.h>
#include <math.h>
typedef struct { float minx, miny, maxx, maxy; } box;
static void swapf(float *a, float *b) { float t = *a; *a = *b; *b = t; }
static int swept_aabb(const box *a, const box *b,
                      float vax, float vay, float vbx, float vby,
                      float *toi)
{
    float vx = vax - vbx, vy = vay - vby;
    float tmin = 0.0f, tmax = 1.0f;
    if (fabsf(vx) < 1e-6f) {
        if (a->maxx < b->minx || a->minx > b->maxx) return 0;
    } else {
        float inv = 1.0f / vx;
        float t1 = (b->minx - a->maxx) * inv;
        float t2 = (b->maxx - a->minx) * inv;
        if (t1 > t2) swapf(&t1, &t2);
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return 0;
    }
    if (fabsf(vy) < 1e-6f) {
        if (a->maxy < b->miny || a->miny > b->maxy) return 0;
    } else {
        float inv = 1.0f / vy;
        float t1 = (b->miny - a->maxy) * inv;
        float t2 = (b->maxy - a->miny) * inv;
        if (t1 > t2) swapf(&t1, &t2);
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return 0;
    }
    *toi = tmin;
    return tmin >= 0.0f && tmin <= 1.0f;
}
int main(void)
{
    box a = {-0.5f, -0.5f, 0.5f, 0.5f};
    box b = {1.5f, -0.5f, 2.5f, 0.5f};
    box c = {1.5f, 3.0f, 2.5f, 4.0f};
    float t;
    int hit = swept_aabb(&a, &b, 2.0f, 0.0f, 0.0f, 0.0f, &t);
    printf("hit=%d t=%.3f\n", hit, hit ? t : -1.0f);
    hit = swept_aabb(&a, &c, 2.0f, 0.0f, 0.0f, 0.0f, &t);
    printf("hit=%d\n", hit);
}

/* ================= EXPLANATION =================
   What it does
     Tests whether two axis-aligned boxes that move with constant velocity
     during a time step will touch. It returns the earliest time of impact
     (TOI) in [0,1], not just whether they overlap now.

   Step by step
     1. Collision depends only on relative motion, so the code subtracts
        b's velocity from a's. The boxes now move with relative velocity
        (vx, vy) while b is treated as static.
     2. On the x axis, the boxes begin to overlap when a's right edge
        reaches b's left edge, and stop when a's left edge passes b's
        right edge. Dividing those distances by vx gives entry and exit
        times t1, t2. The code sorts them so t1 is the entry.
     3. tmin is the latest entry time over all axes; tmax is the earliest
        exit time. If tmin <= tmax, the intervals overlap and the boxes
        touch. If tmin > tmax, one axis separates them first.
     4. The process repeats for y. If either relative velocity component
        is near zero, the code checks whether the boxes overlap on that
        axis for the whole step instead of dividing by zero.
     5. For a at x=0 moving right at 2 and b at x=2, the gap is 1.0 and
        the entry time is 0.5. The second box is far above, so no hit.

   Output
     "hit=1 t=0.500"
     "hit=0"

   Why it matters
     Discrete overlap tests miss fast objects: a bullet can jump entirely
     past a thin wall between frames. Swept AABB (the slab method) finds
     the exact first contact time, so the simulation can stop the object
     at the wall, spawn an impact, and avoid tunneling. It is a standard
     broadphase/narrowphase tool in 2D games, and the same slab math
     extends to 3D and rays.

   Caveats
     - This is the 2D AABB version. The 3D version adds a z slab and uses
       the same tmin/tmax intersection.
     - It assumes constant velocity over the step; acceleration or
       rotation needs subdivision or a different method.
     - The boxes must be axis-aligned. Rotated boxes need OBB tests or a
       separating-axis approach.
     - When boxes already overlap at t=0, tmin is 0; the caller must
       resolve the penetration rather than treating it as a future hit.
     - The epsilon on zero velocity is scale-dependent; a tiny relative
       velocity should still be handled by the general path.
     - A hit gives a time, not a contact normal; compute the face that
       produced tmin to resolve the collision correctly.
   ============================================== */
