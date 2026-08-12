#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H
#include <stdint.h>
typedef struct {
    uint16_t voltage_mv;
    uint8_t percent;
    uint8_t present;
    uint8_t charging;
    uint8_t low;
    uint8_t critical;
    uint32_t updated_at_ms;
} Battery_Data_t;
void BatteryManager_Init(void);
void BatteryManager_Process(void);
void BatteryManager_ForceUpdate(void);
const Battery_Data_t *BatteryManager_Get(void);
uint8_t BatteryManager_TakeCriticalRequest(void);
#endif
