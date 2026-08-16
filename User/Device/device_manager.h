#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdint.h>

typedef struct {
    uint8_t connected;
    float temperature;
    float humidity;
    uint32_t updated_at_ms;
    uint32_t next_update_ms;
} Device_EnvironmentData_t;

typedef struct {
    uint8_t connected;
    uint8_t measuring;
    uint16_t raw;
    uint16_t bpm;
    uint32_t updated_at_ms;
    uint32_t next_update_ms;
} Device_HeartData_t;

typedef struct {
    uint8_t connected;
    uint32_t steps_today;
    uint32_t updated_at_ms;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temperature_x100;
} Device_MotionData_t;

typedef struct {
    uint8_t connected;
    uint16_t direction_deg;
} Device_CompassData_t;

void DeviceManager_Init(void);
void DeviceManager_Process(void);
void DeviceManager_ForceEnvironmentUpdate(void);
void DeviceManager_EnvironmentOpen(void);
void DeviceManager_EnvironmentClose(void);
void DeviceManager_StartHeartMeasurement(void);
void DeviceManager_StopHeartMeasurement(void);
void DeviceManager_UpdateMotion(void);
void DeviceManager_MotionOpen(void);
void DeviceManager_MotionClose(void);
void DeviceManager_CompassOpen(void);
void DeviceManager_UpdateCompass(void);
void DeviceManager_CompassClose(void);

const Device_EnvironmentData_t *DeviceManager_GetEnvironment(void);
const Device_HeartData_t *DeviceManager_GetHeart(void);
const Device_MotionData_t *DeviceManager_GetMotion(void);
const Device_CompassData_t *DeviceManager_GetCompass(void);

uint8_t DeviceManager_GetWristWakeEnabled(void);
void DeviceManager_SetWristWakeEnabled(uint8_t enabled);
uint8_t DeviceManager_GetAmbientEnabled(void);
void DeviceManager_SetAmbientEnabled(uint8_t enabled);
uint8_t DeviceManager_TakeWristRaiseEvent(void);
uint8_t DeviceManager_TakeWristLowerEvent(void);
uint8_t DeviceManager_CheckWristAfterStop(void);
uint8_t DeviceManager_GetWorkingBrightness(void);
uint8_t DeviceManager_GetAmbientBrightness(void);
void DeviceManager_SetBrightness(uint8_t working_percent,
                                 uint8_t ambient_percent);
uint8_t DeviceManager_GetWatchFace(void);
void DeviceManager_SetWatchFace(uint8_t index);
void DeviceManager_SaveDateTimeNow(void);
uint8_t DeviceManager_CanEnterStop(void);

#endif
