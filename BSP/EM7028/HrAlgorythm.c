#include "HrAlgorythm.h"
#include "string.h"

Queue datas;
Queue times;
Queue HR_List;

void HR_AlgoInit(void)
{
    /* 清空峰值、时间和平均滤波状态，开始一段独立测量。 */
	initQueue(&datas);
	initQueue(&times);
	initQueue(&HR_List);
}

uint8_t Hr_Ave_Filter(uint32_t *HrList, uint8_t lenth)
{
    /* 对最近若干心率值求平均，减小单个峰间隔误差。 */
	uint32_t ave;
	uint8_t i;
	for(i = 0;i<lenth;i++)
	{
		ave += HrList[i];
	}
	ave /= lenth;
}

uint16_t HR_Calculate(uint16_t present_dat,uint32_t present_time)
{
    /* 根据光电波形阈值交叉检测脉搏峰，并由相邻峰时间差换算 BPM。 */

	static uint16_t peaks_time[]={0,0};
	static uint8_t HR=0;

	if(isQueueFull(&datas))
	{dequeue(&datas);}
	if(isQueueFull(&times))
	{dequeue(&times);}
	if(isQueueFull(&HR_List))
	{dequeue(&HR_List);}

	enqueue(&datas,present_dat);
	enqueue(&times,present_time);

	if((datas.data[3]>=datas.data[2]) && (datas.data[3]>=datas.data[1]) && (datas.data[3]>datas.data[0]) 
		&& (datas.data[3]>=datas.data[4]) && (datas.data[3]>=datas.data[5]) && (datas.data[3]>datas.data[6]))
	{
			if((times.data[3]-peaks_time[0])>425)
			{
					peaks_time[1] = peaks_time[0];
					peaks_time[0] = times.data[3];
					enqueue(&HR_List,60000/(peaks_time[0]-peaks_time[1]));
					if(HR_List.data[6]!=0)
					{HR = Hr_Ave_Filter(HR_List.data,7);}
			}
	}
	return HR;
}



