#include "attitude.h"

#include <math.h>

static quat_t mul(quat_t a, quat_t b)
{
    return (quat_t){
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

static quat_t normalize(quat_t q)
{
    const float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return n > 1e-9f ? (quat_t){q.w / n, q.x / n, q.y / n, q.z / n} : (quat_t){1, 0, 0, 0};
}

/* The rotation by rotation vector `r` (axis times radians). */
static quat_t from_rotation(vec3_t r)
{
    const float angle = v3_len(r);
    if (angle < 1e-9f) {
        return (quat_t){1.0f, 0.5f * r.x, 0.5f * r.y, 0.5f * r.z};
    }
    const float s = sinf(0.5f * angle) / angle;
    return (quat_t){cosf(0.5f * angle), r.x * s, r.y * s, r.z * s};
}

/* The shortest rotation taking unit vector `from` onto unit vector `to`. */
static quat_t between(vec3_t from, vec3_t to)
{
    const vec3_t c = v3_cross(from, to);
    const float d = v3_dot(from, to);
    if (d < -0.9999f) {
        /* Opposite: half a turn about any axis square to `from`. */
        const vec3_t axis =
            fabsf(from.x) < 0.9f ? v3_cross(from, v3(1, 0, 0)) : v3_cross(from, v3(0, 1, 0));
        const float n = v3_len(axis);
        return (quat_t){0.0f, axis.x / n, axis.y / n, axis.z / n};
    }
    return normalize((quat_t){1.0f + d, c.x, c.y, c.z});
}

void attitude_init(attitude_t *a)
{
    a->q = (quat_t){1.0f, 0.0f, 0.0f, 0.0f};
    a->primed = false;
}

vec3_t attitude_to_room(const attitude_t *a, vec3_t v)
{
    const quat_t p = {0.0f, v.x, v.y, v.z};
    const quat_t c = {a->q.w, -a->q.x, -a->q.y, -a->q.z};
    const quat_t r = mul(mul(a->q, p), c);
    return v3(r.x, r.y, r.z);
}

void attitude_feed(attitude_t *a, vec3_t rotation, vec3_t gravity)
{
    const float g = v3_len(gravity);
    if (g < 1e-3f) {
        return;
    }
    const vec3_t up = v3_scale(gravity, -1.0f / g);
    if (!a->primed) {
        /* Level the hat's up with the room's, with no turn about it, so
         * LED 0's side starts towards the room's x axis. */
        a->q = between(up, v3(0.0f, 0.0f, 1.0f));
        a->primed = true;
        return;
    }
    /* The rotation is in hat axes: it acts on the hat's side. */
    a->q = normalize(mul(a->q, from_rotation(rotation)));
    /* Pull the hat's up, seen in the room, a share of the way to the
     * room's up. */
    const vec3_t seen = attitude_to_room(a, up);
    const vec3_t axis = v3_cross(seen, v3(0.0f, 0.0f, 1.0f));
    const float s = v3_len(axis);
    if (s > 1e-6f) {
        const float angle = atan2f(s, seen.z) * ATTITUDE_GRAVITY_GAIN;
        a->q = normalize(mul(from_rotation(v3_scale(axis, angle / s)), a->q));
    }
}
