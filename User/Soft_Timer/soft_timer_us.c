#include "soft_timer_us.h"


#define MAX_SOFT_US_TIMER    8U
#define SYSTICK_LOAD_VAL    79999U
#define SYSTICK_CNT_PER_US  80U

/*
 * 该实现假设 SysTick 时钟为 80 MHz 且 LOAD=79999；修改系统时钟后必须同步
 * 更新这两个常量，否则“微秒”会产生比例误差。
 */
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
    /* 所有槽共享一次初始 tick/VAL 快照，随后每槽独立维护自己的时间基准。 */
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
    /* 注册也相当于重新启动：即使槽之前启用，也从当前时刻重新计周期。 */
    if(id >= MAX_SOFT_US_TIMER || period_us == 0 || callback == NULL)
        return false;

    SoftUsTimer_TypeDef *p_tim = &soft_us_timer[id];
    p_tim->enable      = 1;
    p_tim->period_us   = period_us;
    p_tim->callback    = callback;

    /* 无论之前是否启用，都复位计时基准和累计值。 */
    p_tim->total_us_cnt= 0;
    p_tim->last_tick   = HAL_GetTick();
    p_tim->last_val    = SysTick->VAL;

    return true;
}

void softUsTimer_Stop(uint8_t id)
{
    /* 停止不删除 callback，但 enable=0 后不会再调用；重新注册会覆盖它。 */
    if(id < MAX_SOFT_US_TIMER)
    {
        SoftUsTimer_TypeDef *p_tim = &soft_us_timer[id];
        p_tim->enable = 0;
        p_tim->total_us_cnt = 0;
    }
}

void softUsTimer_TickHandler(void)
{
    /* 该函数必须高频轮询；回调在调用者上下文执行，不属于硬件中断。 */
    uint32_t curr_tick = HAL_GetTick();
    uint32_t now_val   = SysTick->VAL;

    for(uint8_t i = 0; i < MAX_SOFT_US_TIMER; i++)
    {
        SoftUsTimer_TypeDef *p_tim = &soft_us_timer[i];
        if(!p_tim->enable)
            continue;

        /* 1. HAL tick 差给出两次处理间隔中的完整毫秒部分。 */
        uint32_t tick_diff = curr_tick - p_tim->last_tick;
        // 整毫秒换算微秒
        uint32_t total_us = tick_diff * 1000U;

        /* 2. SysTick 向下计数，用 VAL 差补出当前毫秒内的微秒细分。 */
        uint32_t val_delta;
        if(now_val < p_tim->last_val)
        {
            /* VAL 穿过 0 并重新装载 LOAD 时，差值要跨越重装点。 */
            val_delta = (p_tim->last_val - now_val) + SYSTICK_LOAD_VAL;
        }
        else
        {
            val_delta = p_tim->last_val - now_val;
        }
        uint32_t sub_us = val_delta / SYSTICK_CNT_PER_US;
        total_us += sub_us;

        /* 3. 把本轮经过时间累加进该槽的周期余量。 */
        p_tim->total_us_cnt += total_us;

        /* 4. 保存快照，下一轮只计算新增时间。 */
        p_tim->last_tick = curr_tick;
        p_tim->last_val  = now_val;

        /* 5. 到期执行一次并保留超出部分；当前代码不会 while 补发多次。 */
        if(p_tim->total_us_cnt >= p_tim->period_us)
        {
            p_tim->callback();
            p_tim->total_us_cnt -= p_tim->period_us;
        }
    }
}

/* 弱空回调可被应用层同名强实现覆盖。 */
__attribute__((weak)) void softUsTimer_0_Callback(void){}
__attribute__((weak)) void softUsTimer_1_Callback(void){}
__attribute__((weak)) void softUsTimer_2_Callback(void){}
__attribute__((weak)) void softUsTimer_3_Callback(void){}
__attribute__((weak)) void softUsTimer_4_Callback(void){}
__attribute__((weak)) void softUsTimer_5_Callback(void){}
__attribute__((weak)) void softUsTimer_6_Callback(void){}
__attribute__((weak)) void softUsTimer_7_Callback(void){}
