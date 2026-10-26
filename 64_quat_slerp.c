/* MAGIC: interpolate 3D rotations on the unit quaternion sphere, taking
   the shorter arc, instead of interpolating Euler angles. */
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846
typedef struct { double w, x, y, z; } quat;
static quat qaxis(double angle, double x, double y, double z)
{
    double h = angle * 0.5, s = sin(h);
    return (quat){cos(h), x * s, y * s, z * s};
}
static quat qnorm(quat q)
{
    double n = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    return (quat){q.w/n, q.x/n, q.y/n, q.z/n};
}
static quat slerp(quat a, quat b, double t)
{
    double d = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
    if (d < 0.0) { b.w = -b.w; b.x = -b.x; b.y = -b.y; b.z = -b.z; d = -d; }
    if (d > 0.9995) {
        quat r = {a.w + t*(b.w-a.w), a.x + t*(b.x-a.x),
                  a.y + t*(b.y-a.y), a.z + t*(b.z-a.z)};
        return qnorm(r);
    }
    double th = acos(d), s = sin(th);
    double wa = sin((1.0 - t) * th) / s;
    double wb = sin(t * th) / s;
    return (quat){wa*a.w + wb*b.w, wa*a.x + wb*b.x,
                  wa*a.y + wb*b.y, wa*a.z + wb*b.z};
}
int main(void)
{
    quat a = qaxis(0.0, 0, 0, 1);
    quat b = qaxis(PI / 2.0, 0, 0, 1);
    quat m = slerp(a, b, 0.5);
    printf("slerp(0.5) = %.6f %.6f %.6f %.6f\n", m.w, m.x, m.y, m.z);
}

/* ================= EXPLANATION =================
   What it does
     Interpolates between two 3D orientations represented as unit
     quaternions. `a` is the identity, `b` is a 90-degree rotation about
     the z axis, and slerp at t = 0.5 returns the 45-degree rotation on
     the shortest arc between them.

   Step by step
     1. qaxis converts an axis-angle rotation to a quaternion:
        q = (cos(angle/2), axis * sin(angle/2)). For 90 degrees about z,
        b = (0.707107, 0, 0, 0.707107).
     2. The dot product d measures how close the two orientations are on
        the 4D unit sphere. If d is negative, the quaternions are in
        opposite hemispheres (q and -q represent the same rotation);
        negating one and d makes the interpolation take the shorter arc.
     3. For nearly identical orientations, sin(theta) is tiny and the
        spherical formula is numerically unstable, so the code falls back
        to a normalised linear interpolation.
     4. Otherwise theta = acos(d) is the angle between the quaternions,
        and the weights sin((1-t)theta)/sin(theta) and
        sin(t theta)/sin(theta) move along the great circle. At t = 0.5,
        the result is the 45-degree rotation:
        (0.923880, 0, 0, 0.382683).
     5. The result is unit length, so it is a valid rotation.

   Output
     "slerp(0.5) = 0.923880 0.000000 0.000000 0.382683"

   Why it matters
     Euler angles suffer from gimbal lock and interpolate along strange
     paths. Quaternions compose rotations without singularities, and slerp
     gives constant angular velocity along the shortest arc - essential
     for camera turns, skeletal animation, and networked orientation.
     Game engines, robotics, and spacecraft attitude control all use this
     math.

   Caveats
     - The inputs must be normalised unit quaternions; slerp assumes it.
     - q and -q are the same rotation, but the dot-product sign flip is
       essential for the shortest path.
     - Slerp is not commutative and is not a simple component-wise blend;
       nlerp (normalised lerp) is cheaper but has non-constant speed.
     - The near-parallel branch uses a fixed threshold (0.9995); choose it
       based on precision.
     - For animating more than two orientations, use squad or a
       neighbourhood-aware scheme to avoid sudden direction changes.
   ============================================== */
