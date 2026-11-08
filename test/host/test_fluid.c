#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "fluid.h"
#include "unity.h"

static fluid_t f;

void setUp(void)
{
    fluid_init(&f);
    fluid_populate(&f);
}

void tearDown(void) {}

static const vec3_t NO_GRAVITY = {0.0f, 0.0f, 0.0f};
static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

#define AXIS_X (FLUID_NX / 2.0f)
#define AXIS_Y (FLUID_NY / 2.0f)

static void run(int steps, vec3_t accel)
{
    for (int i = 0; i < steps; i++) {
        fluid_step(&f, accel);
    }
}

static vec3_t mean_position(void)
{
    vec3_t sum = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < f.n; i++) {
        sum = v3_add(sum, f.p[i].pos);
    }
    return v3_scale(sum, 1.0f / (float)f.n);
}

static float radius_of(vec3_t p)
{
    return hypotf(p.x - AXIS_X, p.y - AXIS_Y);
}

static float mean_radius(void)
{
    float sum = 0.0f;
    for (int i = 0; i < f.n; i++) {
        sum += radius_of(f.p[i].pos);
    }
    return sum / (float)f.n;
}

/* Whether `p` lies in a cell of the hat: its centre within the hat's
 * radius at its height, and below HAT_HEIGHT. */
static bool in_can(vec3_t p)
{
    if (p.x < 0.0f || p.y < 0.0f || p.z < 0.0f || floorf(p.z) + 0.5f >= HAT_HEIGHT) {
        return false;
    }
    const float cx = floorf(p.x) + 0.5f, cy = floorf(p.y) + 0.5f, cz = floorf(p.z) + 0.5f;
    return hypotf(cx - AXIS_X, cy - AXIS_Y) < hat_radius_at(cz);
}

/* Highest particle: the surface. */
static float top(void)
{
    float z = 0.0f;
    for (int i = 0; i < f.n; i++) {
        z = fmaxf(z, f.p[i].pos.z);
    }
    return z;
}

/* The grid holds the hat, and has room for half a hat of particles. */
static void test_grid_fits_can(void)
{
    TEST_ASSERT_TRUE(2.0f * HAT_RADIUS_MAX <= FLUID_NX);
    TEST_ASSERT_TRUE(2.0f * HAT_RADIUS_MAX <= FLUID_NY);
    TEST_ASSERT_TRUE(HAT_HEIGHT <= FLUID_NZ);
    TEST_ASSERT_TRUE(HAT_HEIGHT > FLUID_NZ - 1);
    TEST_ASSERT_LESS_THAN_INT(FLUID_PARTICLES, f.n);
}

/* Particles fill the bottom half, inside the round wall. */
static void test_populate_fills_bottom_half(void)
{
    TEST_ASSERT_GREATER_THAN_INT(FLUID_PARTICLES / 2, f.n);
    for (int i = 0; i < f.n; i++) {
        TEST_ASSERT_TRUE(in_can(f.p[i].pos));
    }
    TEST_ASSERT_FLOAT_WITHIN(1.5f, HAT_HEIGHT / 2.0f, top());
}

/* At rest the fluid stays in the bottom half and keeps its volume. */
static void test_settles_in_bottom_half(void)
{
    run(300, DOWN);
    TEST_ASSERT_FLOAT_WITHIN(0.6f, HAT_HEIGHT / 4.0f, mean_position().z);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, AXIS_X, mean_position().x);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, AXIS_Y, mean_position().y);
    TEST_ASSERT_FLOAT_WITHIN(1.5f, HAT_HEIGHT / 2.0f, top());
}

/* Gravity along +x, the hat on its side: the fluid lies along the +x side
 * of the wall, the full height of the hat. */
static void test_on_its_side_fluid_lies_along_the_wall(void)
{
    run(300, (vec3_t){1.0f, 0.0f, 0.0f});
    const vec3_t m = mean_position();
    TEST_ASSERT_GREATER_THAN_FLOAT(AXIS_X + 0.6f, m.x);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, AXIS_Y, m.y);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, HAT_HEIGHT / 2.0f, m.z);
}

static void test_stays_inside_can_whichever_way_gravity_points(void)
{
    const vec3_t dirs[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},           {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}, {0.6f, -0.6f, 0.5f}, {-3, 2, 1},
    };
    for (size_t d = 0; d < sizeof dirs / sizeof dirs[0]; d++) {
        run(40, dirs[d]);
        for (int i = 0; i < f.n; i++) {
            const vec3_t p = f.p[i].pos;
            TEST_ASSERT_TRUE(in_can(p));
        }
    }
}

/* After a step, flow into and out of every cell that held fluid when the
 * step started balances. */
static void test_pressure_solve_removes_divergence(void)
{
    const int NX = FLUID_NX, NY = FLUID_NY, NZ = FLUID_NZ;
    const vec3_t tilted = {0.7f, 0.0f, -0.7f};
    run(20, tilted);
    bool fluid[FLUID_CELLS] = {false};
    for (int i = 0; i < f.n; i++) {
        const vec3_t p = f.p[i].pos;
        fluid[(int)p.x + NX * ((int)p.y + NY * (int)p.z)] = true;
    }
    float density[FLUID_CELLS];
    memcpy(density, f.density, sizeof density);
    fluid_step(&f, tilted);

    float worst = 0.0f;
    for (int k = 0; k < NZ; k++) {
        for (int j = 0; j < NY; j++) {
            for (int i = 0; i < NX; i++) {
                const int cell = i + NX * (j + NY * k);
                /* Cells packed above rest density get outflow on purpose. */
                if (!fluid[cell] || density[cell] > FLUID_PARTICLES_PER_CELL) {
                    continue;
                }
                const float div = f.vel[0][(i + 1) + (NX + 1) * (j + NY * k)] -
                                  f.vel[0][i + (NX + 1) * (j + NY * k)] +
                                  f.vel[1][i + NX * ((j + 1) + (NY + 1) * k)] -
                                  f.vel[1][i + NX * (j + (NY + 1) * k)] +
                                  f.vel[2][i + NX * (j + NY * (k + 1))] -
                                  f.vel[2][i + NX * (j + NY * k)];
                worst = fmaxf(worst, fabsf(div));
            }
        }
    }
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, worst);
}

/* The hat starts spinning about its axis: the fluid lags behind (on the +x
 * side it moves towards -y), and keeps out at the wall. */
static void test_spin_lags_and_flings_outwards(void)
{
    const float radius_before = mean_radius();
    fluid_set_rotation(&f, (vec3_t){0.0f, 0.0f, 0.15f});
    fluid_step(&f, NO_GRAVITY);
    float vy_east = 0.0f;
    for (int i = 0; i < f.n; i++) {
        const vec3_t p = f.p[i].pos;
        if (p.x > AXIS_X + 1.0f && fabsf(p.y - AXIS_Y) < 1.0f) {
            vy_east += f.p[i].vel.y;
        }
    }
    TEST_ASSERT_LESS_THAN_FLOAT(0.0f, vy_east);

    run(200, NO_GRAVITY);
    TEST_ASSERT_GREATER_THAN_FLOAT(radius_before * 1.05f, mean_radius());
}

/* Tints run across and up the fluid, inside the spread, and moving fluid
 * shifts hue. */
static void test_tint_varies_across_fluid_and_with_speed(void)
{
    float west, east, still;
    fluid_sample(&f, (vec3_t){AXIS_X - 1.5f, AXIS_Y, 0.5f}, &west);
    fluid_sample(&f, (vec3_t){AXIS_X + 1.5f, AXIS_Y, 0.4f * HAT_HEIGHT}, &east);
    TEST_ASSERT_LESS_THAN_FLOAT(east - 20.0f, west);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-FLUID_TINT_SPREAD, west);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(FLUID_TINT_SPREAD, east);

    /* A push sideways: near the surface, where the fluid can move. */
    const vec3_t near_top = {AXIS_X + 1.5f, AXIS_Y, 0.4f * HAT_HEIGHT};
    fluid_sample(&f, near_top, &still);
    for (int i = 0; i < 3; i++) {
        fluid_step(&f, (vec3_t){3.0f, 0.0f, 0.0f});
    }
    float moving;
    fluid_sample(&f, near_top, &moving);
    TEST_ASSERT_GREATER_THAN_FLOAT(still + 10.0f, moving);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_grid_fits_can);
    RUN_TEST(test_populate_fills_bottom_half);
    RUN_TEST(test_settles_in_bottom_half);
    RUN_TEST(test_on_its_side_fluid_lies_along_the_wall);
    RUN_TEST(test_stays_inside_can_whichever_way_gravity_points);
    RUN_TEST(test_pressure_solve_removes_divergence);
    RUN_TEST(test_spin_lags_and_flings_outwards);
    RUN_TEST(test_tint_varies_across_fluid_and_with_speed);
    return UNITY_END();
}
