#include "soft_timer.h"

/* 最多支持 8 个毫秒软件定时器；本模块不使用动态内存。 */
#define MAX_SOFT_TIMER      8

typedef struct {
    uint8_t id;               /* 固定槽号。 */
    uint8_t enable;           /* 1=参与 TickHandler 计数。 */
    uint32_t period_ms;       /* 期望周期，单位取决于 TickHandler 调用节拍。 */
    uint32_t tick_cnt;        /* 从本周期起点累计的 tick 数。 */
    void (*callback)(void);   /* 到期后在调用 TickHandler 的上下文执行。 */
}SoftTimer_TypeDef;

static SoftTimer_TypeDef soft_timer[MAX_SOFT_TIMER] = {0};

void softTimer_init(void) {
    /* 初始化全部固定槽；注册前任何槽都不会触发回调。 */
    for(uint8_t i = 0; i < MAX_SOFT_TIMER; i++) {   
        soft_timer[i].id = i;
        soft_timer[i].enable = 0;
        soft_timer[i].period_ms = 0;
        soft_timer[i].tick_cnt = 0;
        soft_timer[i].callback = NULL;
    }
}

bool softTimer_Register(uint8_t id, uint32_t period_ms, void (*callback)(void)) {
    /* 拒绝越界槽、零周期和空回调，避免运行期崩溃。 */
    if(id >= MAX_SOFT_TIMER || period_ms == 0 || callback == NULL) {
        return false;
    }
    SoftTimer_TypeDef *p_timer = &soft_timer[id];
    p_timer->enable = 1;
    p_timer->tick_cnt = 0;
    p_timer->period_ms = period_ms;
    p_timer->callback = callback;
    return true;
}

void softTimer_Stop(uint8_t id) {
    /* Stop 同时清累计值，下次 Register 会从完整新周期开始。 */
    if (id < MAX_SOFT_TIMER) {
        SoftTimer_TypeDef *p_timer = &soft_timer[id];

        p_timer->enable = 0;
        p_timer->tick_cnt = 0;
    }
}

void softTimer_TickHandler(void) {
    /* 重要：只有每 1 ms 调用一次时 period_ms 才真正表示毫秒。 */
    for( uint8_t i = 0; i < MAX_SOFT_TIMER; i++) {
        SoftTimer_TypeDef *p_timer = &soft_timer[i];
        if(p_timer->enable) {
            p_timer->tick_cnt++;
            if(p_timer->tick_cnt >= p_timer->period_ms) {
                p_timer->callback();
                p_timer->tick_cnt = 0;
            }
        }
    }
}

/* 弱定义允许应用层提供同名强符号覆盖，而未使用槽仍可安全链接。 */
__attribute__((weak)) void softTimer_0_Callback(void) {}
__attribute__((weak)) void softTimer_1_Callback(void) {}
__attribute__((weak)) void softTimer_2_Callback(void) {}
__attribute__((weak)) void softTimer_3_Callback(void) {}
__attribute__((weak)) void softTimer_4_Callback(void) {}
__attribute__((weak)) void softTimer_5_Callback(void) {}
__attribute__((weak)) void softTimer_6_Callback(void) {}
__attribute__((weak)) void softTimer_7_Callback(void) {}
