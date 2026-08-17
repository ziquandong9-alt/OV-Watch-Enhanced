#ifndef __SOFT_TIMER_US_H
#define __SOFT_TIMER_US_H

#include <stdbool.h>
#include "main.h"



/* 初始化全部微秒软件定时器槽。 */
void softUsTimer_Init(void);

/* 注册/重启指定槽；period_us 不能为 0，callback 不能为 NULL。 */
bool softUsTimer_Register(uint8_t id, uint32_t period_us, void (*callback)(void));

// 停止指定定时器
void softUsTimer_Stop(uint8_t id);

/* 主循环轮询入口；调用越频繁，微秒定时误差越小。 */
void softUsTimer_TickHandler(void);

// 弱空回调
void softUsTimer_0_Callback(void);
void softUsTimer_1_Callback(void);
void softUsTimer_2_Callback(void);
void softUsTimer_3_Callback(void);
void softUsTimer_4_Callback(void);
void softUsTimer_5_Callback(void);
void softUsTimer_6_Callback(void);
void softUsTimer_7_Callback(void);

#endif

