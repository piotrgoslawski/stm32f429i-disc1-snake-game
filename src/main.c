#include "l3gd20.h"
#include "snake.h"
#include "snake_render.h"
#include "tilt.h"

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(snake, LOG_LEVEL_INF);

/* Stack budget is small; the game state (~1.5 KB body array) must never be
   a local variable. */
static SnakeGame game;
static TiltEstimator tilt;

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static void tilt_reset_after_blocking(TiltEstimator *t, float bias_x, float bias_y, uint32_t now, uint32_t *gyro_last);
static const char *dir_name(SnakeDirection d);

typedef enum {
    BTN_IDLE = 0,
    BTN_DEBOUNCE,
    BTN_PRESSED,
} ButtonFsmState;

/* Edge-detects the user button with a >=30 ms debounce: a raw HIGH arms the
   debounce timer, a drop before 30 ms is treated as a glitch and rearms
   from IDLE, and a HIGH sustained for 30 ms fires the edge exactly once.
   The FSM only rearms once the button is released. */
static bool Button_PollEdge(void)
{
    static ButtonFsmState btn_state = BTN_IDLE;
    static uint32_t btn_change_time = 0;

    bool raw = (gpio_pin_get_dt(&button) != 0);
    uint32_t now = k_uptime_get_32();

    switch (btn_state) {
        case BTN_IDLE:
            if (raw) {
                btn_state = BTN_DEBOUNCE;
                btn_change_time = now;
            }
            break;
        case BTN_DEBOUNCE:
            if (!raw) {
                btn_state = BTN_IDLE;
            } else if (now - btn_change_time >= 30) {
                btn_state = BTN_PRESSED;
                return true;
            }
            break;
        case BTN_PRESSED:
            if (!raw) {
                btn_state = BTN_IDLE;
            }
            break;
    }
    return false;
}

int main(void)
{
    if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&button)) {
        LOG_ERR("GPIO device(s) not ready");
        return 0;
    }
    /* A failed button configure would make gpio_pin_get_dt() return a
       negative errno, which Button_PollEdge() would read as "pressed". */
    if (gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE) != 0 ||
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE) != 0 ||
        gpio_pin_configure_dt(&button, GPIO_INPUT) != 0) {
        LOG_ERR("GPIO pin configuration failed");
        return 0;
    }

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return 0;
    }
    display_blanking_off(display_dev);

    LOG_INF("L3GD20 gyroscope initializing");
    L3GD20_Init();

    Tilt_Init(&tilt, TILT_DEFAULT_MAP);
    SnakeRender_DrawMessage("HOLD STILL");

    /* One-time startup bias calibration: ~1 s of blocking sampling before
       the game loop starts. k_msleep is explicitly permitted here only. */
    float bias_x = 0.0f, bias_y = 0.0f;
    for (int i = 0; i < 100; i++) {
        k_msleep(10);
        float rx, ry, rz;
        L3GD20_ReadDPS(&rx, &ry, &rz);
        bias_x += rx;
        bias_y += ry;
    }
    bias_x /= 100.0f;
    bias_y /= 100.0f;
    Tilt_SetBias(&tilt, bias_x, bias_y);
    LOG_INF("Tilt bias measured: x=%.2f y=%.2f dps", (double)bias_x, (double)bias_y);

    Snake_Init(&game, 0xA5A5A5A5u);
    SnakeRender_DrawPlayfield(&game);
    LOG_INF("Snake game started, score=%u", (unsigned)game.score);

    uint32_t led3_last = 0, led4_last = 0;
    uint32_t tick_last = k_uptime_get_32();
    uint32_t gyro_last;
    uint32_t dbg_last = 0;
    tilt_reset_after_blocking(&tilt, bias_x, bias_y, k_uptime_get_32(), &gyro_last);

    while (1) {
        uint32_t now = k_uptime_get_32();
        if (now - led3_last >= 125) {
            gpio_pin_toggle_dt(&led0);
            led3_last = now;
        }
        if (now - led4_last >= 250) {
            gpio_pin_toggle_dt(&led1);
            led4_last = now;
        }

        if (now - gyro_last >= 10) {
            uint32_t dt_ms = now - gyro_last;
            gyro_last = now;

            float rx, ry, rz;
            L3GD20_ReadDPS(&rx, &ry, &rz);
            Tilt_Update(&tilt, rx, ry, (float)dt_ms / 1000.0f);

            SnakeDirection tilt_dir;
            if (Tilt_Direction(&tilt, &tilt_dir)) {
                if (game.state == SNAKE_STATE_RUNNING) {
                    Snake_SetDirection(&game, tilt_dir);
                }
            }
        }

        if (now - dbg_last >= 500) {
            dbg_last = now;
            LOG_DBG("Tilt angle_x=%.1f angle_y=%.1f dir=%s",
                    (double)tilt.angle_x, (double)tilt.angle_y,
                    tilt.has_last_direction ? dir_name(tilt.last_direction)
                                            : "NEUTRAL");
        }

        if (Button_PollEdge()) {
            if (game.state == SNAKE_STATE_RUNNING) {
                Snake_SetDirection(&game, (SnakeDirection)((game.heading + 1) % 4));
            } else {
                Snake_Init(&game, now);
                SnakeRender_DrawPlayfield(&game);
                tick_last = now;
                tilt_reset_after_blocking(&tilt, bias_x, bias_y, k_uptime_get_32(), &gyro_last);
                LOG_INF("Snake game restarted");
            }
        }

        if (game.state == SNAKE_STATE_RUNNING && (now - tick_last) >= 200) {
            tick_last = now;

            SnakeCell old_tail = game.body[game.length - 1];
            uint16_t prev_score = game.score;

            Snake_Step(&game);

            bool ate = (game.score != prev_score);
            if (game.state == SNAKE_STATE_RUNNING) {
                SnakeRender_UpdateStep(&game, old_tail.x, old_tail.y, ate);
                if (ate) {
                    LOG_INF("Snake ate food, score=%u", (unsigned)game.score);
                }
            } else {
                const char *msg = (game.state == SNAKE_STATE_WON) ? "YOU WIN" : "GAME OVER";
                SnakeRender_DrawEndScreen(&game, msg);
                tilt_reset_after_blocking(&tilt, bias_x, bias_y, k_uptime_get_32(), &gyro_last);
                LOG_INF("Snake %s, score=%u", msg, (unsigned)game.score);
            }
        }

        k_msleep(1);
    }
}

/* Resets the tilt estimator's angles (bias preserved) and re-anchors the
   gyro sample timer, so the blocking LCD redraw that just ran is not
   integrated as rotation on the next sample. */
static void tilt_reset_after_blocking(TiltEstimator *t, float bias_x, float bias_y, uint32_t now, uint32_t *gyro_last)
{
    Tilt_Init(t, TILT_DEFAULT_MAP);
    Tilt_SetBias(t, bias_x, bias_y);
    *gyro_last = now;
}

static const char *dir_name(SnakeDirection d)
{
    switch (d) {
        case SNAKE_DIR_RIGHT: return "RIGHT";
        case SNAKE_DIR_DOWN:  return "DOWN";
        case SNAKE_DIR_LEFT:  return "LEFT";
        case SNAKE_DIR_UP:    return "UP";
    }
    return "?";
}
