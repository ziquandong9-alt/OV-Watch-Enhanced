#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H
#include <stdint.h>
void BLEManager_Init(void);
/* 在主循环中消费 DMA 环形队列并解析按行协议。 */
void BLEManager_Process(void);
/* STOP 前后关闭/恢复 UART DMA 与蓝牙模块。 */
void BLEManager_Suspend(void);
void BLEManager_Resume(void);
uint8_t BLEManager_IsConnected(void);
/* 连接状态每变化一次递增，UI 可据此避免无意义轮询重画。 */
uint32_t BLEManager_GetGeneration(void);
void BLEManager_SendStatus(void);
#endif
