#include "tilt.h"

#include <math.h>

void Tilt_Init(TiltEstimator *t, TiltMap map)
{
    t->angle_x = 0.0f;
    t->angle_y = 0.0f;
    t->bias_x = 0.0f;
    t->bias_y = 0.0f;
    t->map = map;
    t->has_last_direction = false;
}

void Tilt_SetBias(TiltEstimator *t, float bias_x_dps, float bias_y_dps)
{
    t->bias_x = bias_x_dps;
    t->bias_y = bias_y_dps;
}

void Tilt_Update(TiltEstimator *t, float rate_x_dps, float rate_y_dps, float dt_s)
{
    if (dt_s <= 0.0f) {
        return;
    }

    float rx = rate_x_dps - t->bias_x;
    float ry = rate_y_dps - t->bias_y;

    t->angle_x += rx * dt_s;
    t->angle_y += ry * dt_s;

    /* Exponential decay toward 0. Must stay multiplicative (not the linear
       approximation angle -= angle*dt_s/tau), which can overshoot past 0
       and flip sign for a large dt_s, breaking the monotonic-return
       guarantee and firing a spurious opposite direction. */
    float decay = expf(-dt_s / TILT_DECAY_TAU_S);
    t->angle_x *= decay;
    t->angle_y *= decay;

    if (t->angle_x > TILT_MAX_DEG) t->angle_x = TILT_MAX_DEG;
    if (t->angle_x < -TILT_MAX_DEG) t->angle_x = -TILT_MAX_DEG;
    if (t->angle_y > TILT_MAX_DEG) t->angle_y = TILT_MAX_DEG;
    if (t->angle_y < -TILT_MAX_DEG) t->angle_y = -TILT_MAX_DEG;
}

static void tilt_apply_map(const TiltEstimator *t, float *ud, float *lr)
{
    float axis_val[2] = { t->angle_x, t->angle_y };
    *ud = axis_val[t->map.ud_axis] * t->map.ud_sign;
    *lr = axis_val[t->map.lr_axis] * t->map.lr_sign;
}

bool Tilt_Direction(TiltEstimator *t, SnakeDirection *out)
{
    float ud, lr;
    tilt_apply_map(t, &ud, &lr);

    float ud_mag = fabsf(ud);
    float lr_mag = fabsf(lr);
    bool ud_dominant = (ud_mag >= lr_mag);
    float dominant_mag = ud_dominant ? ud_mag : lr_mag;
    bool is_direction = (dominant_mag >= TILT_DEADZONE_DEG);

    SnakeDirection candidate;
    if (ud_dominant) {
        candidate = (ud > 0.0f) ? SNAKE_DIR_DOWN : SNAKE_DIR_UP;
    } else {
        candidate = (lr > 0.0f) ? SNAKE_DIR_RIGHT : SNAKE_DIR_LEFT;
    }

    if (is_direction) {
        if (!t->has_last_direction || t->last_direction != candidate) {
            t->has_last_direction = true;
            t->last_direction = candidate;
            *out = candidate;
            return true;
        }
        return false;
    }

    /* Neutral: forget the last reported direction so the same direction can
       re-fire later. Updating this only inside the return-true branch would
       leave a decayed-to-neutral estimator unable to ever re-report the
       same direction again. */
    if (t->has_last_direction) {
        t->has_last_direction = false;
    }
    return false;
}
