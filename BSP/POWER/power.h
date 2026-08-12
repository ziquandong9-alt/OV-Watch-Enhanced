#ifndef WATCH_POWER_H
#define WATCH_POWER_H
#include <stdint.h>
void Power_Init(void);
uint16_t Power_ReadBatteryMillivolts(void);
uint8_t Power_IsCharging(void);
uint8_t Power_VoltageToPercent(uint16_t millivolts);
#endif
