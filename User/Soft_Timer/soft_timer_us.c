#include "soft_timer_us.h"


#define MAX_SOFT_US_TIMER    8U
#define SYSTICK_LOAD_VAL    79999U
#define SYSTICK_CNT_PER_US  80U


typedef struct {
    uint8_t  id;
    uint8_t  enable;
    uint32_t period_us;    // 定时周期 单位us
    uint32_t total_us_cnt; // 当前累计微秒
    uint32_t last_tick;    // 上次处理时的uwTick(ms基准)
    uint32_t last_val;     // 上次处理时的SysTick VAL(us细分)
    void (*callback)(void);
} SoftUsTimer_TypeDef;

static SoftUsTimer_TypeDef soft_us_timer[MAX_SOFT_US_TIMER] = {0};

void softUsTimer_Init(void)
{
    uint32_t now_tick = HAL_GetTick();
    uint32_t now_val  = SysTick->VAL;

    for(uint8_t i = 0; i < MAX_SOFT_US_TIMER; i++)
    {
        SoftUsTimer_TypeDef *p_tim = &soft_us_timer[i];
        p_tim->id          = i;
        p_tim->enable      = 0;
        p_tim->period_us   = 0;
        p_tim->total_us_cnt= 0;
        p_tim->last_tick   = now_tick;
        p_tim->last_val    = now_val;
        p_tim->callback    = NULL;
    }
}

bool softUsTimer_Register(uint8_t id, uint32_t period_us, void (*callback)(void))
{
    if(id >= MAX_SOFT_US_TIMER || period_us == 0 || callback == NULL)
        return false;

    SoftUsTimer_TypeDef *p_tim = &soft_us_timer[id];
    p_tim->enable      = 1;
    p_tim->period_us   = period_us;
    p_tim->callback    = callback;

    // 无论之前是否启用，强制复位计时基准，刷新倒计时起点
    p_tim->total_us_cnt= 0;
    p_tim->last_tick   = HAL_GetTick();
    p_tim->last_val    = SysTick->VAL;

    return true;
}

void softUsTimer_Stop(uint8_t id)
{
    if(id < MAX_SOFT_US_TIMER)
    {
        SoftUsTimer_TypeDef *p_tim = &soft_us_timer[id];
        p_tim->enable = 0;
        p_tim->total_us_cnt = 0;
    }
}

void softUsTimer_TickHandler(void)
{
    uint32_t curr_tick = HAL_GetTick();
    uint32_t now_val   = SysTick->VAL;

    for(uint8_t i = 0; i < MAX_SOFT_US_TIMER; i++)
    {
        SoftUsTimer_TypeDef *p_tim = &soft_us_timer[i];
        if(!p_tim->enable)
            continue;

        // 1. 计算两次处理间隔的完整毫秒数
        uint32_t tick_diff = curr_tick - p_tim->last_tick;
        // 整毫秒换算微秒
        uint32_t total_us = tick_diff * 1000U;

        // 2. 计算当前毫秒内SysTick走过的计数值，换算剩余us
        uint32_t val_delta;
        if(now_val < p_tim->last_val)
        {
            // 跨0重装溢出
            val_delta = (p_tim->last_val - now_val) + SYSTICK_LOAD_VAL;
        }
        else
        {
            val_delta = p_tim->last_val - now_val;
        }
        uint32_t sub_us = val_delta / SYSTICK_CNT_PER_US;
        total_us += sub_us;

        // 3. 累加总流逝微秒
        p_tim->total_us_cnt += total_us;

        // 4. 更新本次基准，给下一轮计算使用
        p_tim->last_tick = curr_tick;
        p_tim->last_val  = now_val;

        // 5. 判断是否到达定时周期，支持一次卡顿触发多次回调
        if(p_tim->total_us_cnt >= p_tim->period_us)
        {
            p_tim->callback();
            p_tim->total_us_cnt -= p_tim->period_us;
        }
    }
}

// 空回调实现
__attribute__((weak)) void softUsTimer_0_Callback(void){}
__attribute__((weak)) void softUsTimer_1_Callback(void){}
__attribute__((weak)) void softUsTimer_2_Callback(void){}
__attribute__((weak)) void softUsTimer_3_Callback(void){}
__attribute__((weak)) void softUsTimer_4_Callback(void){}
__attribute__((weak)) void softUsTimer_5_Callback(void){}
__attribute__((weak)) void softUsTimer_6_Callback(void){}
__attribute__((weak)) void softUsTimer_7_Callback(void){}
