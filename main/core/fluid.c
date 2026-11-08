#include "fluid.h"

#include <math.h>
#include <string.h>

#define NX FLUID_NX
#define NY FLUID_NY
#define NZ FLUID_NZ

/* Centre of the hat's round wall, across the grid. */
#define AXIS_X (NX / 2.0f)
#define AXIS_Y (NY / 2.0f)

/* Multiplier on the hat's rotation forces (Euler, Coriolis, centrifugal).
 * 1.0 is the real thing. */
#define ROTATION_GAIN 1.0f

/* Particle radius in cells: particles closer than twice this are pushed
 * apart, and they stay this far from the walls. */
#define PARTICLE_RADIUS 0.3f

/* Pushing-apart passes per step. */
#define SEPARATION_PASSES 1

/* Gauss-Seidel passes of the pressure solve, and its over-relaxation. The
 * hat's 14-cell column needs 50 passes to settle still. */
#define PRESSURE_ITERATIONS 60
#define OVER_RELAXATION     1.9f

/* How strongly cells packed above the rest density push particles out
 * again, so the fluid keeps its volume over time. */
#define DRIFT_STIFFNESS 1.0f

/* Share of FLIP (change in grid velocity) against PIC (grid velocity) in
 * the velocity particles take back: lower is calmer and more viscous. */
#define FLIP_RATIO 0.8f

#define NONE UINT16_MAX

/* A cell's flags in fluid_t.open: whether it lies in the hat, and which
 * of its faces are open, with cells in the hat on both sides. Closed
 * faces are the hat's walls. */
#define OPEN_IN 0x40

/* A face, in fluid_t.face: between two cells of the hat, a wall (one cell
 * of the hat), or outside it (none). Faces beyond the grid's last cell,
 * past the end of an axis shorter than FLUID_FACES, stay FACE_OUTSIDE. */
#define FACE_OUTSIDE 0
#define FACE_WALL    1
#define FACE_OPEN    2
#define OPEN_X0      0x01
#define OPEN_X1      0x02
#define OPEN_Y0      0x04
#define OPEN_Y1      0x08
#define OPEN_Z0      0x10
#define OPEN_Z1      0x20

/* The 13 neighbouring cells "after" a cell (in z, then y, then x order).
 * Checking a cell against itself and these visits every pair of adjacent
 * cells exactly once. */
static const int8_t FORWARD_NEIGHBORS[13][3] = {
    {1, 0, 0},  {-1, 1, 0}, {0, 1, 0}, {1, 1, 0},  {-1, -1, 1}, {0, -1, 1}, {1, -1, 1},
    {-1, 0, 1}, {0, 0, 1},  {1, 0, 1}, {-1, 1, 1}, {0, 1, 1},   {1, 1, 1},
};

/* 1/s for the pressure solve, where s is how many of a cell's six faces
 * are open (3 in a corner, 6 inside). */
static const float INV_OPEN[7] = {0.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 0.2f, 1.0f / 6.0f};

static inline float axis(vec3_t v, int a)
{
    return a == 0 ? v.x : a == 1 ? v.y : v.z;
}

static inline void set_axis(vec3_t *v, int a, float value)
{
    if (a == 0) {
        v->x = value;
    } else if (a == 1) {
        v->y = value;
    } else {
        v->z = value;
    }
}

static inline int cell_index(int x, int y, int z)
{
    return x + NX * (y + NY * z);
}

/* Face (i, j, k) of the faces carrying component a: the index along axis
 * a runs to the grid size, the others to one less. */
static inline int face_index(int a, int i, int j, int k)
{
    const int nx = NX + (a == 0), ny = NY + (a == 1);
    return i + nx * (j + ny * k);
}

static inline int cell_of(float v, int n)
{
    const int c = (int)v;
    return c < 0 ? 0 : c >= n ? n - 1 : c;
}

/* Trilinear weights of a point over the 8 nearest samples of a grid whose
 * samples sit at integer coordinates plus `offset` per axis, with `size`
 * samples per axis. */
typedef struct {
    int idx[8];
    float w[8];
} stencil_t;

static void stencil(vec3_t pos, const float offset[3], const int size[3], stencil_t *s)
{
    /* Per axis: the two samples either side and their weights. A sample
     * beyond the grid (past a wall) gets no weight rather than having it
     * folded into the sample inside, which would make cells along the
     * walls look packed. */
    int at[3][2];
    float w[3][2];
    for (int d = 0; d < 3; d++) {
        const float g = axis(pos, d) - offset[d];
        const int b = (int)(g + 1.0f) - 1; /* floor for g > -1 */
        const float t = g - (float)b;
        for (int k = 0; k < 2; k++) {
            const int i = b + k;
            const bool inside = i >= 0 && i < size[d];
            at[d][k] = inside ? i : 0;
            w[d][k] = inside ? (k ? t : 1.0f - t) : 0.0f;
        }
    }
    for (int c = 0; c < 8; c++) {
        const int dx = c & 1, dy = (c >> 1) & 1, dz = c >> 2;
        s->idx[c] = at[0][dx] + size[0] * (at[1][dy] + size[1] * at[2][dz]);
        s->w[c] = w[0][dx] * w[1][dy] * w[2][dz];
    }
}

/* Stencil over the faces carrying component a: they sit on the cell
 * boundary along a and at the cell centre along the other two axes. */
static void face_stencil(int a, vec3_t pos, stencil_t *s)
{
    const float offset[3] = {a == 0 ? 0.0f : 0.5f, a == 1 ? 0.0f : 0.5f, a == 2 ? 0.0f : 0.5f};
    const int size[3] = {NX + (a == 0), NY + (a == 1), NZ + (a == 2)};
    stencil(pos, offset, size, s);
}

static void cell_stencil(vec3_t pos, stencil_t *s)
{
    static const float offset[3] = {0.5f, 0.5f, 0.5f};
    static const int size[3] = {NX, NY, NZ};
    stencil(pos, offset, size, s);
}

/* Push a particle back from a closed face along one axis: `lo` and `hi`
 * are its cell's faces, `open_lo` and `open_hi` whether they are open. */
static void collide_axis(float *pos, float *vel, float lo, float hi, bool open_lo, bool open_hi)
{
    if (!open_lo && *pos < lo + PARTICLE_RADIUS) {
        *pos = lo + PARTICLE_RADIUS;
        if (*vel < 0.0f) {
            *vel = 0.0f;
        }
    } else if (!open_hi && *pos > hi - PARTICLE_RADIUS) {
        *pos = hi - PARTICLE_RADIUS;
        if (*vel > 0.0f) {
            *vel = 0.0f;
        }
    }
}

/* Keep a particle off the hat's walls, which are the closed faces of its
 * cells; a wall stops motion into it. A particle that cut a corner into a
 * cell outside the hat first goes back into the nearest cell of the hat
 * beside, above or below it, a step towards the hat's middle along one
 * axis or two. */
static void collide_walls(const fluid_t *f, particle_t *p)
{
    int i = cell_of(p->pos.x, NX), j = cell_of(p->pos.y, NY), k = cell_of(p->pos.z, NZ);
    if (!(f->open[cell_index(i, j, k)] & OPEN_IN)) {
        static const int8_t STEPS[6][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                                           {1, 1, 0}, {1, 0, 1}, {0, 1, 1}};
        const int di = p->pos.x < AXIS_X ? 1 : -1;
        const int dj = p->pos.y < AXIS_Y ? 1 : -1;
        const int dk = p->pos.z < HAT_HEIGHT / 2.0f ? 1 : -1;
        for (int n = 0; n < 6; n++) {
            const int ti = i + STEPS[n][0] * di, tj = j + STEPS[n][1] * dj;
            const int tk = k + STEPS[n][2] * dk;
            if (ti >= 0 && ti < NX && tj >= 0 && tj < NY && tk >= 0 && tk < NZ &&
                (f->open[cell_index(ti, tj, tk)] & OPEN_IN)) {
                i = ti;
                j = tj;
                k = tk;
                break;
            }
        }
        p->pos.x = clampf(p->pos.x, (float)i, (float)i + 1.0f);
        p->pos.y = clampf(p->pos.y, (float)j, (float)j + 1.0f);
        p->pos.z = clampf(p->pos.z, (float)k, (float)k + 1.0f);
    }
    const uint8_t open = f->open[cell_index(i, j, k)];
    collide_axis(&p->pos.x, &p->vel.x, (float)i, (float)i + 1.0f, open & OPEN_X0, open & OPEN_X1);
    collide_axis(&p->pos.y, &p->vel.y, (float)j, (float)j + 1.0f, open & OPEN_Y0, open & OPEN_Y1);
    collide_axis(&p->pos.z, &p->vel.z, (float)k, (float)k + 1.0f, open & OPEN_Z0, open & OPEN_Z1);
}

/* Fast 1/sqrt(x) for x > 0: a bit-level estimate refined by two Newton
 * steps, multiplies only. The FPU has no single-instruction divide or
 * square root; pushing particles apart runs this thousands of times a
 * frame. */
static inline float inv_sqrt(float x)
{
    union {
        float f;
        uint32_t i;
    } u = {.f = x};
    u.i = 0x5f3759dfu - (u.i >> 1);
    float y = u.f;
    const float half = 0.5f * x;
    y = y * (1.5f - half * y * y);
    y = y * (1.5f - half * y * y);
    return y;
}

/* Push particles a and b apart if they overlap. */
static inline void separate(particle_t *p, int a, int b)
{
    const float min_dist = 2.0f * PARTICLE_RADIUS;
    const vec3_t delta = v3_sub(p[b].pos, p[a].pos);
    const float d2 = v3_dot(delta, delta);
    if (d2 < min_dist * min_dist && d2 > 1e-8f) {
        const float inv = inv_sqrt(d2);
        const float push = (min_dist - d2 * inv) * 0.5f;
        const vec3_t offset = v3_scale(delta, push * inv);
        p[a].pos = v3_sub(p[a].pos, offset);
        p[b].pos = v3_add(p[b].pos, offset);
    }
}

/* Sort particles into cells, and count them per cell. */
static void bin_particles(fluid_t *f)
{
    for (int i = 0; i < FLUID_CELLS; i++) {
        f->head[i] = NONE;
        f->count[i] = 0;
    }
    for (int i = 0; i < f->n; i++) {
        const vec3_t pos = f->p[i].pos;
        const int cell = cell_index(cell_of(pos.x, NX), cell_of(pos.y, NY), cell_of(pos.z, NZ));
        f->next[i] = f->head[cell];
        f->head[cell] = (uint16_t)i;
        f->count[cell]++;
    }
}

/* One pass pushing every overlapping pair apart. Walks the cells, pairing
 * each only with itself and its forward neighbours, so every pair is
 * tested once. */
static void push_apart(fluid_t *f)
{
    bin_particles(f);
    for (int cz = 0; cz < NZ; cz++) {
        for (int cy = 0; cy < NY; cy++) {
            for (int cx = 0; cx < NX; cx++) {
                const int cell = cell_index(cx, cy, cz);
                if (f->head[cell] == NONE) {
                    continue;
                }
                int neighbors[13];
                int count = 0;
                for (int k = 0; k < 13; k++) {
                    const int nx = cx + FORWARD_NEIGHBORS[k][0];
                    const int ny = cy + FORWARD_NEIGHBORS[k][1];
                    const int nz = cz + FORWARD_NEIGHBORS[k][2];
                    if (nx >= 0 && nx < NX && ny >= 0 && ny < NY && nz < NZ) {
                        const int neighbor = cell_index(nx, ny, nz);
                        if (f->head[neighbor] != NONE) {
                            neighbors[count++] = neighbor;
                        }
                    }
                }
                for (uint16_t a = f->head[cell]; a != NONE; a = f->next[a]) {
                    for (uint16_t b = f->next[a]; b != NONE; b = f->next[b]) {
                        separate(f->p, a, b);
                    }
                    for (int k = 0; k < count; k++) {
                        for (uint16_t b = f->head[neighbors[k]]; b != NONE; b = f->next[b]) {
                            separate(f->p, a, b);
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < f->n; i++) {
        collide_walls(f, &f->p[i]);
    }
}

/* Spread particle velocities onto the faces. Walls carry no flow, and
 * faces with no cell of the hat on either side take no part, just as if
 * they lay beyond the grid. */
static void particles_to_grid(fluid_t *f)
{
    memset(f->vel, 0, sizeof f->vel);
    memset(f->weight, 0, sizeof f->weight);
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        const particle_t *p = &f->p[i];
        for (int a = 0; a < 3; a++) {
            face_stencil(a, p->pos, &s);
            const float v = axis(p->vel, a);
            for (int c = 0; c < 8; c++) {
                f->vel[a][s.idx[c]] += s.w[c] * v;
                f->weight[a][s.idx[c]] += s.w[c];
            }
        }
    }
    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < FLUID_FACES; i++) {
            if (f->weight[a][i] > 0.0f) {
                f->vel[a][i] /= f->weight[a][i];
            }
        }
    }
    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < FLUID_FACES; i++) {
            if (f->face[a][i] != FACE_OPEN) {
                f->vel[a][i] = 0.0f;
            }
            if (f->face[a][i] == FACE_OUTSIDE) {
                f->weight[a][i] = 0.0f;
            }
        }
    }
    memcpy(f->prev, f->vel, sizeof f->prev);
}

/* Make the flow out of every fluid cell zero (Gauss-Seidel with
 * over-relaxation), against closed faces and an open surface. Cells packed
 * above the rest density get a little outflow, which spreads them out
 * again. */
static void solve_pressure(fluid_t *f)
{
    const float rest = (float)FLUID_PARTICLES_PER_CELL;
    for (int it = 0; it < PRESSURE_ITERATIONS; it++) {
        for (int k = 0; k < NZ; k++) {
            for (int j = 0; j < NY; j++) {
                for (int i = 0; i < NX; i++) {
                    const int cell = cell_index(i, j, k);
                    if (f->count[cell] == 0) {
                        continue;
                    }
                    const int x0 = face_index(0, i, j, k), x1 = face_index(0, i + 1, j, k);
                    const int y0 = face_index(1, i, j, k), y1 = face_index(1, i, j + 1, k);
                    const int z0 = face_index(2, i, j, k), z1 = face_index(2, i, j, k + 1);
                    const uint8_t faces = f->open[cell];
                    const float sx0 = (faces & OPEN_X0) != 0, sx1 = (faces & OPEN_X1) != 0;
                    const float sy0 = (faces & OPEN_Y0) != 0, sy1 = (faces & OPEN_Y1) != 0;
                    const float sz0 = (faces & OPEN_Z0) != 0, sz1 = (faces & OPEN_Z1) != 0;
                    const int open = (int)(sx0 + sx1 + sy0 + sy1 + sz0 + sz1);
                    float div = f->vel[0][x1] - f->vel[0][x0] + f->vel[1][y1] - f->vel[1][y0] +
                                f->vel[2][z1] - f->vel[2][z0];
                    const float compression = f->density[cell] - rest;
                    if (compression > 0.0f) {
                        div -= DRIFT_STIFFNESS * compression;
                    }
                    const float p = -div * INV_OPEN[open] * OVER_RELAXATION;
                    f->vel[0][x0] -= sx0 * p;
                    f->vel[0][x1] += sx1 * p;
                    f->vel[1][y0] -= sy0 * p;
                    f->vel[1][y1] += sy1 * p;
                    f->vel[2][z0] -= sz0 * p;
                    f->vel[2][z1] += sz1 * p;
                }
            }
        }
    }
}

/* Give the particles back the grid velocity: FLIP adds the change the
 * pressure solve made, PIC takes the grid velocity outright. Only faces
 * with particles nearby count. */
static void grid_to_particles(fluid_t *f)
{
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        for (int a = 0; a < 3; a++) {
            face_stencil(a, p->pos, &s);
            float sum = 0.0f, pic = 0.0f, change = 0.0f;
            for (int c = 0; c < 8; c++) {
                const int idx = s.idx[c];
                if (f->weight[a][idx] > 0.0f) {
                    sum += s.w[c];
                    pic += s.w[c] * f->vel[a][idx];
                    change += s.w[c] * (f->vel[a][idx] - f->prev[a][idx]);
                }
            }
            if (sum > 0.0f) {
                const float inv = 1.0f / sum;
                const float flip = axis(p->vel, a) + change * inv;
                set_axis(&p->vel, a, (1.0f - FLIP_RATIO) * pic * inv + FLIP_RATIO * flip);
            }
        }
    }
}

/* Particle density and tint at the cell centres. The pressure solve of
 * the next step uses the same density: particles do not move in
 * between. */
static void update_density(fluid_t *f)
{
    memset(f->density, 0, sizeof f->density);
    memset(f->tint, 0, sizeof f->tint);
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        const particle_t *p = &f->p[i];
        const float tint = p->tint + FLUID_SPEED_TINT * v3_len(p->vel);
        cell_stencil(p->pos, &s);
        for (int c = 0; c < 8; c++) {
            f->density[s.idx[c]] += s.w[c];
            f->tint[s.idx[c]] += s.w[c] * tint;
        }
    }
    for (int i = 0; i < FLUID_CELLS; i++) {
        if (f->density[i] > 0.0f) {
            f->tint[i] /= f->density[i];
        }
    }
}

/* Whether cell (i, j, k) lies in the hat: its centre inside the hat's
 * radius at its height, and below HAT_HEIGHT. */
static bool inside(int i, int j, int k)
{
    if (i < 0 || i >= NX || j < 0 || j >= NY || k < 0 || k >= NZ) {
        return false;
    }
    const float dx = (float)i + 0.5f - AXIS_X, dy = (float)j + 0.5f - AXIS_Y;
    const float z = (float)k + 0.5f, r = hat_radius_at(z);
    return dx * dx + dy * dy < r * r && z < HAT_HEIGHT;
}

void fluid_init(fluid_t *f)
{
    memset(f, 0, sizeof *f);
    for (int a = 0; a < 3; a++) {
        const int di = a == 0, dj = a == 1, dk = a == 2;
        for (int k = 0; k < NZ + dk; k++) {
            for (int j = 0; j < NY + dj; j++) {
                for (int i = 0; i < NX + di; i++) {
                    const int in = inside(i, j, k) + inside(i - di, j - dj, k - dk);
                    f->face[a][face_index(a, i, j, k)] = in == 2   ? FACE_OPEN
                                                         : in == 1 ? FACE_WALL
                                                                   : FACE_OUTSIDE;
                }
            }
        }
    }
    for (int k = 0; k < NZ; k++) {
        for (int j = 0; j < NY; j++) {
            for (int i = 0; i < NX; i++) {
                if (!inside(i, j, k)) {
                    continue;
                }
                f->open[cell_index(i, j, k)] =
                    (uint8_t)(OPEN_IN | (inside(i - 1, j, k) ? OPEN_X0 : 0) |
                              (inside(i + 1, j, k) ? OPEN_X1 : 0) |
                              (inside(i, j - 1, k) ? OPEN_Y0 : 0) |
                              (inside(i, j + 1, k) ? OPEN_Y1 : 0) |
                              (inside(i, j, k - 1) ? OPEN_Z0 : 0) |
                              (inside(i, j, k + 1) ? OPEN_Z1 : 0));
            }
        }
    }
}

void fluid_populate(fluid_t *f)
{
    /* Two particles per cell, on opposite corners of a smaller cube in
     * it: evenly spread and clear of the walls. */
    static const float OFFSETS[2] = {0.3f, 0.7f};
    const int layers = (int)lrintf(HAT_HEIGHT / 2.0f);
    f->n = 0;
    for (int z = 0; z < layers; z++) {
        for (int y = 0; y < NY; y++) {
            for (int x = 0; x < NX; x++) {
                if (!(f->open[cell_index(x, y, z)] & OPEN_IN)) {
                    continue;
                }
                for (int k = 0; k < FLUID_PARTICLES_PER_CELL && f->n < FLUID_PARTICLES; k++) {
                    const float o = OFFSETS[k % 2];
                    const vec3_t pos = v3((float)x + o, (float)y + o, (float)z + o);
                    /* Shades run across the hat and up it: from -1 on the
                     * -x side at the floor to +1 on the +x side at the
                     * half-full level. */
                    const float across = (pos.x - AXIS_X) / HAT_RADIUS_MAX;
                    const float up = pos.z / (0.5f * HAT_HEIGHT);
                    const float t = clampf(0.5f * (across + up), -1.0f, 1.0f);
                    f->p[f->n++] = (particle_t){
                        .pos = pos,
                        .vel = v3(0.0f, 0.0f, 0.0f),
                        .tint = FLUID_TINT_SPREAD * t,
                    };
                }
            }
        }
    }
    update_density(f);
}

void fluid_set_rotation(fluid_t *f, vec3_t rotation)
{
    f->rotation = rotation;
}

void fluid_step(fluid_t *f, vec3_t accel)
{
    const vec3_t gravity = v3_scale(accel, FLUID_GRAVITY);
    const vec3_t center = v3(AXIS_X, AXIS_Y, HAT_HEIGHT / 2.0f);
    /* In the turning hat the fluid feels the Euler force (the spin
     * speeding up), Coriolis and centrifugal forces. */
    const vec3_t spin = v3_scale(f->rotation, ROTATION_GAIN);
    const vec3_t spin_up = v3_sub(spin, v3_scale(f->last_rotation, ROTATION_GAIN));
    f->last_rotation = f->rotation;

    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        const vec3_t r = v3_sub(p->pos, center);
        vec3_t a = gravity;
        a = v3_sub(a, v3_cross(spin_up, r));
        a = v3_sub(a, v3_scale(v3_cross(spin, p->vel), 2.0f));
        a = v3_sub(a, v3_cross(spin, v3_cross(spin, r)));
        p->vel = v3_add(p->vel, a);
    }

    /* Make the velocity divergence-free before moving, so a fluid at rest
     * stays at rest. */
    bin_particles(f);
    particles_to_grid(f);
    solve_pressure(f);
    grid_to_particles(f);

    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        p->vel.x = clampf(p->vel.x, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.y = clampf(p->vel.y, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.z = clampf(p->vel.z, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->pos = v3_add(p->pos, p->vel);
        collide_walls(f, p);
    }
    for (int pass = 0; pass < SEPARATION_PASSES; pass++) {
        push_apart(f);
    }
    update_density(f);
}

float fluid_sample(const fluid_t *f, vec3_t pos, float *tint)
{
    stencil_t s;
    cell_stencil(pos, &s);
    float density = 0.0f, tinted = 0.0f;
    for (int c = 0; c < 8; c++) {
        const float d = s.w[c] * f->density[s.idx[c]];
        density += d;
        tinted += d * f->tint[s.idx[c]];
    }
    if (tint) {
        *tint = density > 0.0f ? tinted / density : 0.0f;
    }
    return density;
}
