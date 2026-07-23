#include "main.h"
#include "stm32f429i_discovery.h"
#include "log.h"
#include "l3gd20.h"

SPI_HandleTypeDef hspi5;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void SPI5_Init(void);
static void USART1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    USART1_Init();
    Log_Init(&huart1);
    SPI5_Init();
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4);

    LOG_TRACE("SystemClock configured (168 MHz SYSCLK)");
    LOG_DEBUG("SPI5 initialized (LCD + gyroscope)");
    LOG_INFO("ILI9341 LCD initializing");

    ILI9341_Init(&hspi5);
    ILI9341_FillScreen(ILI9341_CYAN);

    LOG_INFO("L3GD20 gyroscope initializing");
    L3GD20_Init(&hspi5);

    /* Draw "Hello World!" centered on the 320x240 display */
    const char *msg = "HELLO WORLD!";
    /* 12 chars * 8px/char = 96px wide, centered at x=(320-96)/2=112 */
    ILI9341_DrawString(112, 116, msg, ILI9341_RED, ILI9341_CYAN);

    LOG_INFO("HELLO WORLD! drawn on LCD");
    LOG_WARN("example warning-level message");
    LOG_ERROR("example error-level message");
    LOG_FATAL("example fatal-level message (non-halting demo)");

    uint32_t led3_last = 0, led4_last = 0, gyro_last = 0;
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
        if (now - gyro_last >= 1000) {
            float gx, gy, gz;
            L3GD20_ReadDPS(&gx, &gy, &gz);
            LOG_INFO("gyro: x=%.2f dps y=%.2f dps z=%.2f dps", gx, gy, gz);
            gyro_last = now;
        }
    }
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
