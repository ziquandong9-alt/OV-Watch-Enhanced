#ifndef KEY_H
#define KEY_H

#include "main.h"

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_KEY1_PRESSED
} Key_Event_t;

void Key_Init(void);
void Key_Proc(void);
Key_Event_t Key_GetEvent(void);
void Key_IgnoreUntilRelease(void);

#endif /* KEY_H */
