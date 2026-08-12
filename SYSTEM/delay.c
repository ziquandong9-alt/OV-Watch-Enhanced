#include "delay.h"


void delay_init(void)
{
    /*
     * 不重新配置 SysTick。
     * SysTick 由 FreeRTOS 初始化和管理。
     */
    SystemCoreClockUpdate();
}


void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

void delay_us(uint32_t us)
{
    if (us == 0U)
        return;

    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t reload = SysTick->LOAD;

    /* SysTick 尚未启动，继续等待会成为死循环 */
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) == 0U)
        return;

    const uint32_t required_ticks = us * cycles_per_us;

    uint32_t elapsed_ticks = 0U;
    uint32_t previous = SysTick->VAL;

    while (elapsed_ticks < required_ticks)
    {
        uint32_t current = SysTick->VAL;

        if (previous >= current)
        {
            elapsed_ticks += previous - current;
        }
        else
        {
            /* SysTick 从 0 重装为 LOAD */
            elapsed_ticks += previous + 1U + reload - current;
        }

        previous = current;
    }
}
