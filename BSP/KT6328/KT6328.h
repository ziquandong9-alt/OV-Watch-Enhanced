#ifndef KT6328_H
#define KT6328_H
/* 控制 KT6328 硬件使能脚；串口协议由 BLEManager 负责。 */
void KT6328_Init(void);
void KT6328_Enable(void);
void KT6328_Disable(void);
#endif
