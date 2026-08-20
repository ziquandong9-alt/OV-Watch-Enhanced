#ifndef CONTROL_CENTER_PAGE_H
#define CONTROL_CENTER_PAGE_H

#include <stdint.h>

/* 创建/销毁从表盘上滑进入的快捷控制中心。 */
void ControlCenterPage_Create(void);
void ControlCenterPage_Destroy(void);
/* 手电筒工作时禁止普通 10 秒无操作息屏，必须由实体键退出。 */
uint8_t ControlCenterPage_IsFlashlightActive(void);

#endif
