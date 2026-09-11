#ifndef TILT_H
#define TILT_H

#include <stdbool.h>
#include <stdint.h>

#include "snake.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TILT_DEADZONE_DEG  15.0f
#define TILT_MAX_DEG       45.0f
#define TILT_DECAY_TAU_S   3.0f

typedef struct {
    uint8_t ud_axis;  /* 0 = X, 1 = Y */
    uint8_t lr_axis;
    float   ud_sign;  /* +1/-1: positive maps to DOWN */
    float   lr_sign;  /* +1/-1: positive maps to RIGHT */
} TiltMap;

/* Determined empirically on hardware by dipping each screen edge in turn
   and watching which way the snake steers. Re-measured on the Zephyr port
   (2026-09-11, rev E01+ board with I3G4250D, picture rotated by
   snake_render.c):

       TOP edge down    -> angle_y positive
       BOTTOM edge down -> angle_y negative
       LEFT edge down   -> angle_x positive
       RIGHT edge down  -> angle_x negative

   Hence up/down is driven by sensor Y inverted (negative = DOWN) and
   left/right by sensor X inverted (negative = RIGHT). The HAL-era firmware
   (2026-09-10) had ud_sign = +1; up/down came out reversed after the port
   and only this sign was flipped. Do not infer this from the gyro package
   orientation on the PCB -- the LCD panel is physically rotated to get
   landscape, so the screen axes do not line up with the sensor axes. */
#define TILT_DEFAULT_MAP ((TiltMap){ .ud_axis = 1, .lr_axis = 0, .ud_sign = -1.0f, .lr_sign = -1.0f })

typedef struct {
    float angle_x, angle_y;
    float bias_x, bias_y;
    TiltMap map;
    bool has_last_direction;
    SnakeDirection last_direction;
} TiltEstimator;

/* Resets angles and bias to 0, stores the axis map, and clears the last
   reported direction back to neutral. */
void Tilt_Init(TiltEstimator *t, TiltMap map);

/* Stores the zero-rate offset (degrees/second) to subtract from every
   subsequent Tilt_Update sample. */
void Tilt_SetBias(TiltEstimator *t, float bias_x_dps, float bias_y_dps);

/* Subtracts bias, integrates the rate into angle over dt_s, applies an
   exponential decay toward 0, and clamps to +-TILT_MAX_DEG. No-op when
   dt_s <= 0. */
void Tilt_Update(TiltEstimator *t, float rate_x_dps, float rate_y_dps, float dt_s);

/* Maps the current angles through the axis map and reports a direction only
   when it differs from the last reported reading (neutral -> direction, or
   direction -> different direction). Returns false while neutral or while
   the same direction persists. */
bool Tilt_Direction(TiltEstimator *t, SnakeDirection *out);

#ifdef __cplusplus
}
#endif

#endif /* TILT_H */
