#include "soft_timer.h"

#define MAX_SOFT_TIMER      8   // 最多支持8个软件定时器

typedef struct {
    uint8_t id;        // 定时器ID
    uint8_t enable;  // 定时器是否启用
    uint32_t period_ms;  // 定时周期
    uint32_t tick_cnt;  // 定时计数
    void (*callback)(void);  // 定时器回调函数
}SoftTimer_TypeDef;

static SoftTimer_TypeDef soft_timer[MAX_SOFT_TIMER] = {0};

void softTimer_init(void) {
    for(uint8_t i = 0; i < MAX_SOFT_TIMER; i++) {   
        soft_timer[i].id = i;
        soft_timer[i].enable = 0;
        soft_timer[i].period_ms = 0;
        soft_timer[i].tick_cnt = 0;
        soft_timer[i].callback = NULL;
    }
}

bool softTimer_Register(uint8_t id, uint32_t period_ms, void (*callback)(void)) {
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
    if (id < MAX_SOFT_TIMER) {
        SoftTimer_TypeDef *p_timer = &soft_timer[id];

        p_timer->enable = 0;
        p_timer->tick_cnt = 0;
    }
}

void softTimer_TickHandler(void) {
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

__attribute__((weak)) void softTimer_0_Callback(void) {}
__attribute__((weak)) void softTimer_1_Callback(void) {}
__attribute__((weak)) void softTimer_2_Callback(void) {}
__attribute__((weak)) void softTimer_3_Callback(void) {}
__attribute__((weak)) void softTimer_4_Callback(void) {}
__attribute__((weak)) void softTimer_5_Callback(void) {}
__attribute__((weak)) void softTimer_6_Callback(void) {}
__attribute__((weak)) void softTimer_7_Callback(void) {}
