#include "tilt.h"
#include "test_util.h"

#include <math.h>

/* Tests pin their own axis map on purpose: TILT_DEFAULT_MAP is a per-board
   hardware calibration constant (it changed once the real mapping was measured
   on the board), so asserting direction semantics against it would make these
   tests fail for a perfectly correct implementation. */
static const TiltMap TEST_MAP = { .ud_axis = 0, .lr_axis = 1, .ud_sign = 1.0f, .lr_sign = 1.0f };

static void test_init_is_neutral(void)
{
    TiltEstimator t;
    TiltMap map = TEST_MAP;
    Tilt_Init(&t, map);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    bool ok = (t.angle_x == 0.0f) && (t.angle_y == 0.0f)
        && (t.bias_x == 0.0f) && (t.bias_y == 0.0f)
        && !t.has_last_direction
        && !fired;
    CHECK(ok);
}

static void test_small_tilt_stays_in_deadzone(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    /* 5 dps for 1 s decays to well under TILT_DEADZONE_DEG. */
    Tilt_Update(&t, 5.0f, 0.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    bool ok = !fired && (fabsf(t.angle_x) < TILT_DEADZONE_DEG);
    CHECK(ok);
}

static void test_crossing_deadzone_triggers(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    /* Large rate on the X axis pushes angle_x well past the dead-zone. */
    Tilt_Update(&t, 100.0f, 0.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    bool ok = fired && (d == SNAKE_DIR_DOWN);
    CHECK(ok);
}

static void test_tilt_right(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, 0.0f, 50.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    CHECK(fired && d == SNAKE_DIR_RIGHT);
}

static void test_tilt_left(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, 0.0f, -50.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    CHECK(fired && d == SNAKE_DIR_LEFT);
}

static void test_tilt_down(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, 50.0f, 0.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    CHECK(fired && d == SNAKE_DIR_DOWN);
}

static void test_tilt_up(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, -50.0f, 0.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    CHECK(fired && d == SNAKE_DIR_UP);
}

static void test_dominant_axis_wins(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    /* Both axes cross the dead-zone, but Y is larger, so RIGHT must win
       even though X alone (DOWN) is also past the threshold. */
    Tilt_Update(&t, 25.0f, 40.0f, 1.0f);

    SnakeDirection d;
    bool fired = Tilt_Direction(&t, &d);

    bool ok = fired && (d == SNAKE_DIR_RIGHT)
        && (fabsf(t.angle_x) >= TILT_DEADZONE_DEG)
        && (fabsf(t.angle_y) > fabsf(t.angle_x));
    CHECK(ok);
}

static void test_decay_returns_to_neutral(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, 100.0f, 0.0f, 1.0f);
    SnakeDirection d;
    bool fired1 = Tilt_Direction(&t, &d);

    /* Hold level (zero rate) for 10 s of simulated time, well past the 3 s
       decay time constant: the estimate must settle back under the
       dead-zone and forget the last-reported direction, not just report
       false because "same as last". */
    for (int i = 0; i < 50; i++) {
        Tilt_Update(&t, 0.0f, 0.0f, 0.2f);
    }
    bool fired2 = Tilt_Direction(&t, &d);
    bool neutral_now = !t.has_last_direction && (fabsf(t.angle_x) < TILT_DEADZONE_DEG);

    /* Re-triggering the same original direction proves the state actually
       went back to neutral (a stuck last-direction could never fire DOWN
       again). */
    Tilt_Update(&t, 100.0f, 0.0f, 1.0f);
    SnakeDirection d2;
    bool fired3 = Tilt_Direction(&t, &d2);

    bool ok = fired1 && (d == SNAKE_DIR_DOWN)
        && !fired2 && neutral_now
        && fired3 && (d2 == SNAKE_DIR_DOWN);
    CHECK(ok);
}

static void test_estimate_is_clamped(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    /* Huge rate would integrate to far beyond TILT_MAX_DEG before clamping. */
    Tilt_Update(&t, 1000.0f, 0.0f, 1.0f);

    bool ok = (t.angle_x == TILT_MAX_DEG);
    CHECK(ok);
}

static void test_bias_is_subtracted(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);
    Tilt_SetBias(&t, 10.0f, 0.0f);

    /* Rate equals the bias, so the net rate after subtraction is zero and
       the angle must not move at all. */
    Tilt_Update(&t, 10.0f, 0.0f, 1.0f);

    bool ok = (t.angle_x == 0.0f);
    CHECK(ok);
}

static void test_direction_reported_once_while_held(void)
{
    TiltEstimator t;
    Tilt_Init(&t, TEST_MAP);

    Tilt_Update(&t, 50.0f, 0.0f, 1.0f);
    SnakeDirection d;
    bool fired1 = Tilt_Direction(&t, &d);
    bool fired2 = Tilt_Direction(&t, &d);
    bool fired3 = Tilt_Direction(&t, &d);

    /* Keep pushing further in the same direction (still well past the
       dead-zone): must still be a single event, not one per update. */
    Tilt_Update(&t, 50.0f, 0.0f, 0.05f);
    bool fired4 = Tilt_Direction(&t, &d);

    bool ok = fired1 && (d == SNAKE_DIR_DOWN)
        && !fired2 && !fired3 && !fired4;
    CHECK(ok);
}

static void test_calibrated_inverted_board(void)
{
    TiltMap inverted = { .ud_axis = 0, .lr_axis = 1, .ud_sign = -1.0f, .lr_sign = -1.0f };

    TiltEstimator t1;
    Tilt_Init(&t1, inverted);
    Tilt_Update(&t1, 50.0f, 0.0f, 1.0f); /* angle_x positive, ud_sign flips it */
    SnakeDirection d1;
    bool fired1 = Tilt_Direction(&t1, &d1);

    TiltEstimator t2;
    Tilt_Init(&t2, inverted);
    Tilt_Update(&t2, 0.0f, 50.0f, 1.0f); /* angle_y positive, lr_sign flips it */
    SnakeDirection d2;
    bool fired2 = Tilt_Direction(&t2, &d2);

    bool ok = fired1 && (d1 == SNAKE_DIR_UP)
        && fired2 && (d2 == SNAKE_DIR_LEFT);
    CHECK(ok);
}

int main(void)
{
    test_init_is_neutral();
    test_small_tilt_stays_in_deadzone();
    test_crossing_deadzone_triggers();
    test_tilt_right();
    test_tilt_left();
    test_tilt_down();
    test_tilt_up();
    test_dominant_axis_wins();
    test_decay_returns_to_neutral();
    test_estimate_is_clamped();
    test_bias_is_subtracted();
    test_direction_reported_once_while_held();
    test_calibrated_inverted_board();

    printf("%d tests, %d failed\n", g_tests, g_failed);
    return g_failed != 0;
}
