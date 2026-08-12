#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

#define POWER_WAKE_KEY    0x01U
#define POWER_WAKE_MPU    0x02U
#define POWER_WAKE_RTC    0x04U
#define POWER_WAKE_TOUCH  0x08U

void PowerManager_Init(void);
void PowerManager_BeginStopSession(void);
void PowerManager_EndStopSession(void);
uint8_t PowerManager_StopOnce(void);
void PowerManager_SuspendUnusedPeripherals(uint8_t for_stop);
void PowerManager_ResumePeripherals(void);

#endif
