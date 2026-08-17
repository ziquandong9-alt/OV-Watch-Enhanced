#include "em70x8.h"

/* EM7028 光电心率前端驱动；这里只提供寄存器配置和原始光强读取。 */

#define CLK_ENABLE __HAL_RCC_GPIOB_CLK_ENABLE();
iic_bus_t EM7028_bus = 
{
	.IIC_SDA_PORT = GPIOB,
	.IIC_SCL_PORT = GPIOB,
	.IIC_SDA_PIN  = GPIO_PIN_13,
	.IIC_SCL_PIN  = GPIO_PIN_14,
};

static void EM7028_LED_Init(void)
{
    /* LED_EN 是器件外部电源/发光使能，测量前必须先拉到有效状态。 */
	GPIO_InitTypeDef gpio = {0};
	__HAL_RCC_GPIOB_CLK_ENABLE();
	gpio.Pin = EM7028_LED_EN_PIN;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_PULLDOWN;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(EM7028_LED_EN_PORT, &gpio);
	HAL_GPIO_WritePin(EM7028_LED_EN_PORT, EM7028_LED_EN_PIN, GPIO_PIN_RESET);
}

uint8_t  EM7028_ReadOneReg(unsigned char RegAddr)
{
    /* 从 EM7028 软件 I2C 地址读取单个寄存器。 */
	unsigned char dat;
	dat = IIC_Read_One_Byte(&EM7028_bus, EM7028_ADDR, RegAddr);
	return dat;
}

void  EM7028_WriteOneReg(unsigned char RegAddr, unsigned char dat)
{
    /* 写配置寄存器；业务层不应绕过本驱动直接操作总线。 */
	IIC_Write_One_Byte(&EM7028_bus, EM7028_ADDR, RegAddr, dat);
}

uint8_t EM7028_Get_ID()
{
    /* 读取 ID_REG 并用于判断是否为预期芯片。 */
	return EM7028_ReadOneReg(ID_REG);
}

uint8_t EM7028_hrs_init()
{
    /* 初始化总线/LED GPIO，复位芯片并写入心率采样参数。 */
	uint8_t i = 5;
	
	CLK_ENABLE;
	IICInit(&EM7028_bus);
	EM7028_LED_Init();
	
	while(EM7028_Get_ID() != 0x36 && i)
	{
		HAL_Delay(100);
		i--;
	}
	if(!i)
	{return 1;}
	EM7028_WriteOneReg(HRS_CFG,0x00);				
	//HRS1_EN, HRS2_dis
	//Heart Beat Measurement is enabled with LED1 turned on and only Red Light Sensor and IR sensor enabled. 
	//When LED1 turned on, the result stores to HRS_DATA0;
	EM7028_WriteOneReg(HRS2_DATA_OFFSET, 0x00);
	//0 offset
	EM7028_WriteOneReg(HRS2_GAIN_CTRL, 0x7f);		
	//HRS2 GAIN = 1
	EM7028_WriteOneReg(HRS1_CTRL, 0x47);
	//HRS1 GAIN =1, HRS1 RANGE =8, HRS1 FREQ = 2.62144MHz (1.5625ms), HRS1 RES = 16 bits, HRS1 mode
	EM7028_WriteOneReg(INT_CTRL, 0x00);
	//LED programmed current = 2.5mA
	return 0;
}

uint8_t EM7028_hrs_Enable()
{
    /* 开启光电通道和 LED，之后才会产生有效 HRS 原始样本。 */
	uint8_t i = 5;
	while(EM7028_Get_ID() != 0x36 && i)
	{
		HAL_Delay(100);
		i--;
	}
	if(!i)
	{
		HAL_GPIO_WritePin(EM7028_LED_EN_PORT, EM7028_LED_EN_PIN, GPIO_PIN_RESET);
		return 1;
	}
	HAL_GPIO_WritePin(EM7028_LED_EN_PORT, EM7028_LED_EN_PIN, GPIO_PIN_SET);
	EM7028_WriteOneReg(HRS_CFG,0x08);
	return 0;
}

uint8_t EM7028_hrs_DisEnable()
{
    /* 关闭测量和 LED，减少持续光照与功耗。 */
	uint8_t i = 5;
	uint8_t result = 0;
	while(EM7028_Get_ID() != 0x36 && i)
	{
		HAL_Delay(100);
		i--;
	}
	if(!i)
	{result = 1;}
	else
	{EM7028_WriteOneReg(HRS_CFG,0x00);}
	HAL_GPIO_WritePin(EM7028_LED_EN_PORT, EM7028_LED_EN_PIN, GPIO_PIN_RESET);
	return result;
}

uint16_t EM7028_Get_HRS1(void)
{
    /* 按低/高寄存器顺序组合 HRS1 的 16 位原始采样值。 */
	uint16_t dat;
	dat = EM7028_ReadOneReg(HRS1_DATA0_H);
	dat <<= 8;
	dat |= EM7028_ReadOneReg(HRS1_DATA0_L);
	return dat;
}
