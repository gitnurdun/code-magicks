/* MAGIC: the six clipping planes are linear combinations of the
   view-projection matrix rows. Test an AABB against them. */
#include <math.h>
#include <stdio.h>
typedef struct { float x, y, z; } vec3;
typedef struct { float a, b, c, d; } plane;
typedef struct { vec3 min, max; } box;
static void normalize(plane *p)
{
    float n = sqrtf(p->a*p->a + p->b*p->b + p->c*p->c);
    p->a /= n; p->b /= n; p->c /= n; p->d /= n;
}
static int visible(const box *box, const plane planes[6])
{
    for (int i = 0; i < 6; i++) {
        const plane *p = &planes[i];
        float x = p->a >= 0 ? box->max.x : box->min.x;
        float y = p->b >= 0 ? box->max.y : box->min.y;
        float z = p->c >= 0 ? box->max.z : box->min.z;
        if (p->a*x + p->b*y + p->c*z + p->d < 0.0f) return 0;
    }
    return 1;
}
int main(void)
{
    float f = 1.0f / tanf(0.5f * 3.14159265f / 2.0f);
    float aspect = 1.0f, near = 1.0f, far = 10.0f;
    float A = (far + near) / (near - far);
    float B = (2.0f * far * near) / (near - far);
    /* Row-major projection matrix rows. */
    float r0[4] = {f / aspect, 0, 0, 0};
    float r1[4] = {0, f, 0, 0};
    float r2[4] = {0, 0, A, B};
    float r3[4] = {0, 0, -1, 0};
    plane planes[6] = {
        {r3[0]+r0[0], r3[1]+r0[1], r3[2]+r0[2], r3[3]+r0[3]}, /* left */
        {r3[0]-r0[0], r3[1]-r0[1], r3[2]-r0[2], r3[3]-r0[3]}, /* right */
        {r3[0]+r1[0], r3[1]+r1[1], r3[2]+r1[2], r3[3]+r1[3]}, /* bottom */
        {r3[0]-r1[0], r3[1]-r1[1], r3[2]-r1[2], r3[3]-r1[3]}, /* top */
        {r3[0]+r2[0], r3[1]+r2[1], r3[2]+r2[2], r3[3]+r2[3]}, /* near */
        {r3[0]-r2[0], r3[1]-r2[1], r3[2]-r2[2], r3[3]-r2[3]}, /* far */
    };
    for (int i = 0; i < 6; i++) normalize(&planes[i]);
    box boxes[3] = {
        {{-0.5f, -0.5f, -5.5f}, {0.5f, 0.5f, -4.5f}},
        {{10.5f, -0.5f, -8.5f}, {11.5f, 0.5f, -7.5f}},
        {{-0.5f, -0.5f, 1.5f}, {0.5f, 0.5f, 2.5f}},
    };
    for (int i = 0; i < 3; i++) printf("box %d visible=%d\n", i, visible(&boxes[i], planes));
}

/* ================= EXPLANATION =================
   What it does
     Extracts the six planes of a perspective camera's view frustum from
     its projection matrix, then tests three axis-aligned boxes. A box is
     rejected if it lies entirely on the negative side of any one plane.

   Step by step
     1. The projection matrix is built for a 90-degree vertical field of
        view, square aspect, near = 1, far = 10, looking down -z. Its rows
        are r0..r3.
     2. In clip space, a point is inside if -w <= x <= w, -w <= y <= w, and
        -w <= z <= w. Substituting the matrix rows gives the six planes:
        left = r3 + r0, right = r3 - r0, bottom = r3 + r1, top = r3 - r1,
        near = r3 + r2, far = r3 - r2. For example, left becomes
        x - z >= 0, i.e. x >= z.
     3. Normalising each plane makes its signed distance meaningful, but
        visibility only needs the sign. The positive vertex of the box is
        the corner farthest along the plane normal: for a positive normal
        component, use max; otherwise min.
     4. If that farthest corner is still behind a plane, every corner is
        behind it, so the box is outside. Otherwise the box may be
        visible; this test is conservative and never rejects a visible
        box.
     5. Box 0 is at z = -5 on the axis, inside. Box 1 at x = 11 is outside
        the right plane (at z near -8 the right bound is x <= 8). Box 2 at
        z = +2 is behind the camera and outside the near plane.

   Output
     "box 0 visible=1"
     "box 1 visible=0"
     "box 2 visible=0"

   Why it matters
     Frustum culling rejects most of a large world before sending draw
     calls or testing triangles. Extracting planes once per camera and
     testing a bounding box is cheap, and hierarchical culling (BVH,
     quadtree) lets whole regions be rejected with one test. The same
     plane math is used for shadow frusta and portal clipping.

   Caveats
     - The extraction assumes a specific clip-space convention (OpenGL:
       z in [-w, w]). Direct3D uses z in [0, w], so the near plane formula
       differs.
     - The row/column convention must match how the matrix is built and
       applied; transposing it silently produces wrong planes.
     - The positive-vertex test is conservative. A box can be outside the
       frustum while not fully outside any single plane (a corner-region
       false positive); that is safe but less efficient.
     - Normalising is optional for sign tests; do it if you need actual
       distances for LOD or sorting.
     - Huge boxes around the camera always pass; clip them or use a
       different representation.
     - Culling is not occlusion: a visible box may still be hidden behind
       geometry.
   ============================================== */
