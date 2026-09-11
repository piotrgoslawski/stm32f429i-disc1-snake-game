#include "main.h"
#include "stm32f429i_discovery.h"
#include "log.h"
#include "l3gd20.h"
#include "snake.h"
#include "snake_render.h"
#include "tilt.h"

SPI_HandleTypeDef hspi5;
UART_HandleTypeDef huart1;

/* Stack budget is small (_Min_Stack_Size = 0x400); the game state (~1.5 KB
   body array) must never be a local variable. */
static SnakeGame game;
static TiltEstimator tilt;

static void SystemClock_Config(void);
static void SPI5_Init(void);
static void USART1_Init(void);
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

    bool raw = (BSP_PB_GetState(BUTTON_KEY) != 0);
    uint32_t now = HAL_GetTick();

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
    HAL_Init();
    SystemClock_Config();
    USART1_Init();
    Log_Init(&huart1);
    Log_SetLevel(LOG_LEVEL_INFO);
    SPI5_Init();
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4);
    BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);

    LOG_TRACE("SystemClock configured (168 MHz SYSCLK)");
    LOG_DEBUG("SPI5 initialized (LCD + gyroscope)");
    LOG_INFO("ILI9341 LCD initializing");

    ILI9341_Init(&hspi5);

    LOG_INFO("L3GD20 gyroscope initializing");
    L3GD20_Init(&hspi5);

    Tilt_Init(&tilt, TILT_DEFAULT_MAP);
    SnakeRender_DrawMessage("HOLD STILL");

    /* One-time startup bias calibration: ~1 s of blocking sampling before
       the game loop starts. HAL_Delay is explicitly permitted here only. */
    float bias_x = 0.0f, bias_y = 0.0f;
    for (int i = 0; i < 100; i++) {
        HAL_Delay(10);
        float rx, ry, rz;
        L3GD20_ReadDPS(&rx, &ry, &rz);
        bias_x += rx;
        bias_y += ry;
    }
    bias_x /= 100.0f;
    bias_y /= 100.0f;
    Tilt_SetBias(&tilt, bias_x, bias_y);
    LOG_INFO("Tilt bias measured: x=%.2f y=%.2f dps", (double)bias_x, (double)bias_y);

    Snake_Init(&game, 0xA5A5A5A5u);
    SnakeRender_DrawPlayfield(&game);
    LOG_INFO("Snake game started, score=%u", (unsigned)game.score);

    uint32_t led3_last = 0, led4_last = 0;
    uint32_t tick_last = HAL_GetTick();
    uint32_t gyro_last;
    uint32_t dbg_last = 0;
    tilt_reset_after_blocking(&tilt, bias_x, bias_y, HAL_GetTick(), &gyro_last);

    while (1) {
        uint32_t now = HAL_GetTick();
        if (now - led3_last >= 125) {
            BSP_LED_Toggle(LED3);
            led3_last = now;
        }
        if (now - led4_last >= 250) {
            BSP_LED_Toggle(LED4);
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
            LOG_DEBUG("Tilt angle_x=%.1f angle_y=%.1f dir=%s",
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
                tilt_reset_after_blocking(&tilt, bias_x, bias_y, HAL_GetTick(), &gyro_last);
                LOG_INFO("Snake game restarted");
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
                    LOG_INFO("Snake ate food, score=%u", (unsigned)game.score);
                }
            } else {
                const char *msg = (game.state == SNAKE_STATE_WON) ? "YOU WIN" : "GAME OVER";
                SnakeRender_DrawEndScreen(&game, msg);
                tilt_reset_after_blocking(&tilt, bias_x, bias_y, HAL_GetTick(), &gyro_last);
                LOG_INFO("Snake %s, score=%u", msg, (unsigned)game.score);
            }
        }
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

/* 168 MHz using 8 MHz HSE: PLL_M=8, PLL_N=336, PLL_P=2, PLL_Q=7 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = 8;
    osc.PLL.PLLN            = 336;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;  /* 42 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;  /* 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* SPI5 master, 8-bit, CPOL=0, CPHA=0, software NSS, 84/8 = ~10.5 MHz */
static void SPI5_Init(void)
{
    hspi5.Instance               = SPI5;
    hspi5.Init.Mode              = SPI_MODE_MASTER;
    hspi5.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi5.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi5.Init.NSS               = SPI_NSS_SOFT;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi5.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi5.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi5) != HAL_OK)
        Error_Handler();
}

/* USART1 TX=PA9, RX=PA10, 115200 8N1. APB2=84 MHz. */
static void USART1_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
