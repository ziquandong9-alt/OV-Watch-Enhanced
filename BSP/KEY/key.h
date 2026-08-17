#ifndef KEY_H
#define KEY_H

#include "main.h"

typedef enum {
    /* 扫描层只产生按下一种事件，页面语义由 AppUI 决定。 */
    KEY_EVENT_NONE = 0,
    KEY_EVENT_KEY1_PRESSED
} Key_Event_t;

void Key_Init(void);
/* 主循环轮询消抖。 */
void Key_Proc(void);
Key_Event_t Key_GetEvent(void);
void Key_IgnoreUntilRelease(void);

#endif /* KEY_H */
