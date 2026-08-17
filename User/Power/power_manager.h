#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

/* 唤醒原因是位图，一次唤醒可能同时包含多个来源。 */
#define POWER_WAKE_KEY    0x01U
#define POWER_WAKE_MPU    0x02U
#define POWER_WAKE_RTC    0x04U
#define POWER_WAKE_TOUCH  0x08U

void PowerManager_Init(void);
/* 进入/结束一段 STOP 会话，主要切换 RTC 轮询周期。 */
void PowerManager_BeginStopSession(void);
void PowerManager_EndStopSession(void);
/* 只执行一次 WFI，醒来后返回 POWER_WAKE_xxx 位图。 */
uint8_t PowerManager_StopOnce(void);
void PowerManager_SuspendUnusedPeripherals(uint8_t for_stop);
void PowerManager_ResumePeripherals(void);

#endif
