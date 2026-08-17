#include "DataSave.h"
#include "string.h"

/******************************************
EEPROM Data description:
[0x00]:0x55 for check
[0x01]:0xAA for check

[0x10]:user wrist setting, HWInterface.IMU.wrist_is_enabled
[0x11]:user ui_APPSy_EN setting

[0x20]:Last Save Day(0-31)
[0x21]:Day Steps

*******************************************/


void EEPROM_Init(void)
{
    /* 对业务层保留的初始化入口，内部只准备 BL24C02 总线。 */
	BL24C02_Init();
}


//to check the Data is right and the EEPROM is working
uint8_t EEPROM_Check(void)
{
    /* 写入并读回测试字节判断器件是否存在；返回 0 表示正常。 */
	uint8_t check_buff[2];
	delay_ms(10);
	BL24C02_Read(0,2,check_buff);
	if(check_buff[0] == 0x55 && check_buff[1] == 0xAA)
	{
		return 0;//check OK
	}
	else
	{
		check_buff[0] = 0x55;
		check_buff[1] = 0xAA;
		delay_ms(10);
		BL24C02_Write(0,2,check_buff);
		memset(check_buff,0,2);
		delay_ms(10);
		BL24C02_Read(0,2,check_buff);
		if(check_buff[0] == 0x55 && check_buff[1] == 0xAA)
			return 0;//check ok
	}
	return 1;//check erro
}


//to Save the settings
uint8_t SettingSave(uint8_t *buf, uint8_t addr, uint8_t lenth)
{
    /* 旧版设置保存接口；新版业务层使用带校验的循环记录。 */
	if(addr > 1 && !EEPROM_Check())
	{
		delay_ms(10);
		BL24C02_Write(addr,lenth,buf);
		return 0;
	}
	return 1;
}


//to Get the settings
uint8_t SettingGet(uint8_t *buf, uint8_t addr, uint8_t lenth)
{
    /* 检查地址/器件后读取指定范围。注意 lenth 是原代码保留拼写。 */
	if(addr > 1 && !EEPROM_Check())
	{
		delay_ms(10);
		BL24C02_Read(addr,lenth,buf);
		return 0;
	}
	return 1;
}
