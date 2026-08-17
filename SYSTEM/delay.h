#ifndef __DELAY_H__
#define __DELAY_H__

#include "main.h"

/* delay_us 不重配 SysTick，只观察当前向下计数值。 */
void delay_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif /* __DELAY_H__ */



