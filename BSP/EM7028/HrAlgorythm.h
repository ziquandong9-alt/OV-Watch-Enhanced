#ifndef __HRALGORYTHM_H__
#define __HRALGORYTHM_H__

#include "main.h"
#include "user_Queue.h"

/* 初始化并运行旧版峰间隔心率算法；当前业务层另有轻量实现。 */
void HR_AlgoInit(void);
uint16_t HR_Calculate(uint16_t present_dat,uint32_t present_time);


#endif
