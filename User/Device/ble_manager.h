#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H
#include <stdint.h>
void BLEManager_Init(void);
void BLEManager_Process(void);
void BLEManager_Suspend(void);
void BLEManager_Resume(void);
uint8_t BLEManager_IsConnected(void);
uint32_t BLEManager_GetGeneration(void);
void BLEManager_SendStatus(void);
#endif
