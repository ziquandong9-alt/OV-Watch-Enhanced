#include "delay.h"


void delay_init(void)
{
    /*
     * 不重新配置 SysTick。
     * 当前工程由 HAL 管理 SysTick；这里仅更新 SystemCoreClock 缓存。
     */
    SystemCoreClockUpdate();
}


void delay_ms(uint32_t ms)
{
    /* 毫秒延时直接复用 HAL tick；调用期间 CPU 忙等，不适合长时间任务。 */
    HAL_Delay(ms);
}

void delay_us(uint32_t us)
{
    /* 微秒延时通过观察 SysTick 向下计数实现，不会重新配置系统节拍。 */
    if (us == 0U)
        return;

    /* 每微秒 CPU 周期数取决于当前 SystemCoreClock。 */
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t reload = SysTick->LOAD;

    /* SysTick 尚未启动，继续等待会成为死循环 */
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) == 0U)
        return;

    /* 注意极大的 us 可能使乘法溢出；本函数只适合短硬件时序延时。 */
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
            /* SysTick 从 0 重装为 LOAD 时分两段累计。 */
            elapsed_ticks += previous + 1U + reload - current;
        }

        previous = current;
    }
}
