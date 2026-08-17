#ifndef __SOFT_TIMER_H
#define __SOFT_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"


/* 固定槽毫秒定时器；period_ms 依赖 TickHandler 每 1 ms 被调用一次。 */
void softTimer_init(void);
bool softTimer_Register(uint8_t id, uint32_t period_ms, void (*callback)(void));
void softTimer_Stop(uint8_t id);

void softTimer_0_Callback(void);
void softTimer_1_Callback(void);
void softTimer_2_Callback(void);
void softTimer_3_Callback(void);
void softTimer_4_Callback(void);
void softTimer_5_Callback(void);
void softTimer_6_Callback(void);
void softTimer_7_Callback(void);


#endif /* __SOFT_TIMER_H */

