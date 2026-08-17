#ifndef __AHT21_H
#define __AHT21_H

#include "stm32f4xx_hal.h"
#include "iic_hal.h"
#include "delay.h"

uint8_t AHT_Read_Status(void);
void AHT_Reset(void);
/* AHT21 接口返回 0 表示成功；Start/ReadMeasurement 支持非阻塞状态机。 */
uint8_t AHT_Init(void);
uint8_t AHT_Read(float *humi, float *temp);
void AHT_StartMeasurement(void);
uint8_t AHT_ReadMeasurement(float *humi, float *temp);

#endif
