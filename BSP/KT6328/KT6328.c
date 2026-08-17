#include "KT6328.h"
#include "stm32f4xx_hal.h"

void KT6328_Init(void)
{
    /* PA8 控制蓝牙模块硬件使能，上电后默认拉高启动。 */
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    KT6328_Enable();
}

/* Enable/Disable 只控制电源脚；UART DMA 的启停由 BLEManager 管理。 */
void KT6328_Enable(void) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET); }
void KT6328_Disable(void) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); }
