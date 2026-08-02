#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/* SPI5: SCK=PF7(AF5), MISO=PF8(AF5), MOSI=PF9(AF5), LCD CS=PC2(out),
 * LCD DC=PD13(out), GYRO CS=PC1(out). SCK/MOSI/MISO and the LCD/gyro CS
 * pins are shared across the ILI9341 LCD and the onboard L3GD20 gyroscope. */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI5)
        return;

    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_SPI5_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* SPI5 SCK=PF7, MISO=PF8, MOSI=PF9 */
    gpio.Pin       = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI5;
    HAL_GPIO_Init(GPIOF, &gpio);

    /* LCD CS=PC2, GYRO CS=PC1, DC=PD13 as push-pull outputs.
     * Both CS lines are active-low: preset them HIGH (deselected) before
     * switching the pins to output mode, so neither device is left
     * selected on the shared bus by the GPIO reset-default ODR=0. */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_SET);

    gpio.Pin   = GPIO_PIN_1 | GPIO_PIN_2;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOD, &gpio);
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI5)
        return;

    __HAL_RCC_SPI5_FORCE_RESET();
    __HAL_RCC_SPI5_RELEASE_RESET();

    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1 | GPIO_PIN_2);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_13);
}

/* USART1: TX=PA9(AF7), RX=PA10(AF7). Note: these pins are also wired to the
 * OTG_FS VBUS-sense/ID lines on this board — do not use the USB OTG
 * connector at the same time as this UART. */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
}
