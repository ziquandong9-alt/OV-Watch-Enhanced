#ifndef STATUS_BAR_H
#define STATUS_BAR_H
#include "lvgl.h"
void StatusBar_Create(lv_obj_t *parent);
void StatusBar_Destroy(void);
void StatusBar_SetVisible(uint8_t visible);
void StatusBar_SetBatteryVisible(uint8_t visible);
#endif
