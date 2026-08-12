#include "battery_manager.h"
#include "notification_manager.h"
#include "power.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define BATTERY_SAMPLE_PERIOD_MS 30000UL
#define BATTERY_LOW_PERCENT 15U
#define BATTERY_CRITICAL_PERCENT 5U
#define BATTERY_PRESENT_MIN_MV 2500U
#define BATTERY_CRITICAL_CONFIRM_COUNT 3U

static Battery_Data_t s_battery;
static uint32_t s_next_sample_ms;
static uint8_t s_low_notified;
static uint8_t s_absent_notified;
static uint8_t s_critical_requested;
static uint8_t s_critical_sample_count;

void BatteryManager_Init(void)
{
    memset(&s_battery, 0, sizeof(s_battery));
    Power_Init();
    BatteryManager_ForceUpdate();
}

void BatteryManager_ForceUpdate(void)
{
    uint8_t previous = s_battery.percent;
    uint16_t mv = Power_ReadBatteryMillivolts();
    uint8_t measured = Power_VoltageToPercent(mv);
    s_battery.charging = Power_IsCharging();
    s_battery.voltage_mv = mv;
    s_battery.present = (mv >= BATTERY_PRESENT_MIN_MV) ? 1U : 0U;
    s_battery.percent = (s_battery.updated_at_ms == 0U) ? measured :
        (uint8_t)(((uint16_t)previous * 3U + measured + 2U) / 4U);
    s_battery.low = ((s_battery.present != 0U) &&
                     (s_battery.percent <= BATTERY_LOW_PERCENT)) ? 1U : 0U;
    s_battery.critical = ((s_battery.present != 0U) &&
                          (s_battery.percent <= BATTERY_CRITICAL_PERCENT)) ? 1U : 0U;
    s_battery.updated_at_ms = HAL_GetTick();
    s_next_sample_ms = s_battery.updated_at_ms + BATTERY_SAMPLE_PERIOD_MS;
    if((s_battery.low != 0U) && (s_battery.charging == 0U) &&
       (s_low_notified == 0U)) {
        if(NotificationManager_Push(NOTIFICATION_TYPE_BATTERY, "Low battery",
           "Battery is low. Charge the watch soon.") != 0U) s_low_notified = 1U;
    }
    if((s_battery.present == 0U) && (s_absent_notified == 0U)) {
        if(NotificationManager_Push(NOTIFICATION_TYPE_BATTERY,
           "Battery not detected",
           "The watch is running from debugger or external power. Low-battery shutdown is disabled.") != 0U)
            s_absent_notified = 1U;
    }
    if(s_battery.present != 0U) s_absent_notified = 0U;
    if((s_battery.percent >= 20U) || (s_battery.charging != 0U)) s_low_notified = 0U;
    if((s_battery.critical != 0U) && (s_battery.charging == 0U)) {
        if(s_critical_sample_count < BATTERY_CRITICAL_CONFIRM_COUNT)
            s_critical_sample_count++;
        if(s_critical_sample_count >= BATTERY_CRITICAL_CONFIRM_COUNT)
            s_critical_requested = 1U;
    }
    else s_critical_sample_count = 0U;
}

void BatteryManager_Process(void)
{
    if((int32_t)(HAL_GetTick() - s_next_sample_ms) >= 0) BatteryManager_ForceUpdate();
}

const Battery_Data_t *BatteryManager_Get(void) { return &s_battery; }

uint8_t BatteryManager_TakeCriticalRequest(void)
{
    uint8_t request = s_critical_requested;
    s_critical_requested = 0U;
    return request;
}
