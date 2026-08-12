#include "AHT21.h"

#define AHT_CLK_ENABLE __HAL_RCC_GPIOB_CLK_ENABLE()
iic_bus_t AHT_bus = 
{
	.IIC_SDA_PORT = GPIOB,
	.IIC_SCL_PORT = GPIOB,
	.IIC_SDA_PIN  = GPIO_PIN_13,
	.IIC_SCL_PIN  = GPIO_PIN_14,
};

uint8_t AHT_Read_Status(void)
{
	uint8_t Byte_first;	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x71);
	IICWaitAck(&AHT_bus);
	Byte_first = IICReceiveByte(&AHT_bus);
	IICSendNotAck(&AHT_bus);
	IICStop(&AHT_bus);	
	return Byte_first;
}

uint8_t AHT_Read_Cal_Enable(void)  //check cal enable bit 
{
	uint8_t val = 0;//ret = 0,
 
  val = AHT_Read_Status();
  if((val & 0x68)==0x08)  //check NOR mode 
		return 1;
  else  
		return 0;
}

void AHT_Reset(void)//AHT21 send 0xBA reset call
{
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x70);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0xBA);
	IICWaitAck(&AHT_bus);
	IICStop(&AHT_bus);
}

uint8_t AHT_Init(void)
{
	AHT_CLK_ENABLE;
	IICInit(&AHT_bus);
	
	delay_ms(40);
	
	if((AHT_Read_Status() & 0x08U) != 0x08U)
	{
		IICStart(&AHT_bus);
		IICSendByte(&AHT_bus,0x70);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0xBE);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0x08);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0x00);
		IICWaitAck(&AHT_bus);
		IICStop(&AHT_bus);
		delay_ms(10);
	}
	//AHT_Reset();
	
	return 0;
}	
 
uint8_t AHT_Read(float *humi, float *temp)
{
	uint8_t cnt=5;
	uint8_t  Byte_1th=0;
	uint8_t  Byte_2th=0;
	uint8_t  Byte_3th=0;
	uint8_t  Byte_4th=0;
	uint8_t  Byte_5th=0;
	uint8_t  Byte_6th=0;
	uint32_t RetuData = 0;
	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x70);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0xAC);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x33);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x00);
	IICWaitAck(&AHT_bus);
	IICStop(&AHT_bus);	
	
	delay_ms(80);
	while(((AHT_Read_Status() & 0x80U) == 0x80U) && (cnt != 0U))
	{
		delay_ms(5);
		cnt--;
		AHT_Read_Status();
	}
	if(!cnt)
	{return 1;}
	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x71);
	IICWaitAck(&AHT_bus);
	Byte_1th = IICReceiveByte(&AHT_bus);
	IICSendAck(&AHT_bus);
	Byte_2th = IICReceiveByte(&AHT_bus);
	IICSendAck(&AHT_bus);
	Byte_3th = IICReceiveByte(&AHT_bus);
	IICSendAck(&AHT_bus);
	Byte_4th = IICReceiveByte(&AHT_bus);
	IICSendAck(&AHT_bus);
	Byte_5th = IICReceiveByte(&AHT_bus);
	IICSendAck(&AHT_bus);
	Byte_6th = IICReceiveByte(&AHT_bus);
	IICSendNotAck(&AHT_bus);
	IICStop(&AHT_bus);
	if((Byte_1th & 0x80U) != 0U)
	{return 1;}

	RetuData = (RetuData|Byte_2th)<<8;
	RetuData = (RetuData|Byte_3th)<<8;
	RetuData = (RetuData|Byte_4th);
	RetuData =RetuData >>4;
	*humi = (RetuData * 1000 >> 20);
	*humi /= 10;
	
	RetuData = 0;
	RetuData = (RetuData|(Byte_4th&0x0f))<<8;
	RetuData = (RetuData|Byte_5th)<<8;
	RetuData = (RetuData|Byte_6th);
	RetuData = RetuData&0xfffff;
	*temp = ((RetuData * 2000 >> 20)- 500);
	*temp /= 10;
	
	return 0;
}

/* Start a conversion without waiting. The result is ready after at least 80 ms. */
void AHT_StartMeasurement(void)
{
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x70);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0xAC);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x33);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x00);
	IICWaitAck(&AHT_bus);
	IICStop(&AHT_bus);
}

/* Read an already completed conversion. This function never delays. */
uint8_t AHT_ReadMeasurement(float *humi, float *temp)
{
	uint8_t data[6];
	uint32_t raw;
	uint8_t index;

	if((humi == NULL) || (temp == NULL)) return 1U;
	if((AHT_Read_Status() & 0x80U) != 0U) return 1U;

	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x71);
	IICWaitAck(&AHT_bus);
	for(index = 0U; index < 6U; index++) {
		data[index] = IICReceiveByte(&AHT_bus);
		if(index < 5U) IICSendAck(&AHT_bus);
		else IICSendNotAck(&AHT_bus);
	}
	IICStop(&AHT_bus);
	if((data[0] & 0x80U) != 0U) return 1U;

	raw = ((uint32_t)data[1] << 12) |
	      ((uint32_t)data[2] << 4) |
	      ((uint32_t)data[3] >> 4);
	*humi = ((float)raw * 100.0f) / 1048576.0f;
	raw = (((uint32_t)data[3] & 0x0FU) << 16) |
	      ((uint32_t)data[4] << 8) | data[5];
	*temp = (((float)raw * 200.0f) / 1048576.0f) - 50.0f;
	return 0U;
}
