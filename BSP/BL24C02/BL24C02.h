#ifndef __BL24C02_H
#define __BL24C02_H

#include "stm32f4xx_hal.h"
#include "iic_hal.h"
#include "delay.h"

/* 7 位 I2C 地址；总容量 256 字节。 */
#define BL_ADDRESS	0x50

void BL24C02_Write(uint8_t addr,uint8_t length,uint8_t buff[]);
void BL24C02_Read(uint8_t addr, uint8_t length, uint8_t buff[]);
void BL24C02_Init(void);

#endif
