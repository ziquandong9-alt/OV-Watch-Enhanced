#include "BL24C02.h"

/* BL24C02 是 2 Kbit（256 字节）I2C EEPROM，本层只提供原始地址读写。 */

#define BL_CLK_ENABLE __HAL_RCC_GPIOA_CLK_ENABLE()

iic_bus_t BL_bus = 
{
	.IIC_SDA_PORT = GPIOA,
	.IIC_SCL_PORT = GPIOA,
	.IIC_SDA_PIN  = GPIO_PIN_11,
	.IIC_SCL_PIN  = GPIO_PIN_12,
};


void BL24C02_Write(uint8_t addr,uint8_t length,uint8_t buff[])
{
    /* 页写完成后芯片仍需内部编程时间，调用方负责等待约 5~6 ms。 */
	IIC_Write_Multi_Byte(&BL_bus, BL_ADDRESS, addr, length, buff);
}


void BL24C02_Read(uint8_t addr, uint8_t length, uint8_t buff[])
{
    /* 从 8 位存储地址开始连续读取 length 字节。 */
	IIC_Read_Multi_Byte(&BL_bus, BL_ADDRESS, addr, length, buff);
}


void BL24C02_Init(void)
{
    /* 初始化 EEPROM 所在的软件 I2C 总线，不会擦除数据。 */
	BL_CLK_ENABLE;
	IICInit(&BL_bus);
}
