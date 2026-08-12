#ifndef __SOFT_TIMER_US_H
#define __SOFT_TIMER_US_H

#include <stdbool.h>
#include "main.h"



// 初始化全部us软件定时器
void softUsTimer_Init(void);

// 注册us定时器
bool softUsTimer_Register(uint8_t id, uint32_t period_us, void (*callback)(void));

// 停止指定定时器
void softUsTimer_Stop(uint8_t id);

// 主循环轮询函数，必须放在while(1)里循环调用
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

