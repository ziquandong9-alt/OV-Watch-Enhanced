#include "KT6328.h"
#include "stm32f4xx_hal.h"

void KT6328_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    KT6328_Enable();
}

void KT6328_Enable(void) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET); }
void KT6328_Disable(void) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); }
