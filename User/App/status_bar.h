#ifndef STATUS_BAR_H
#define STATUS_BAR_H
#include "lvgl.h"
/* 状态栏依附 parent 创建，内部每 2 秒刷新 BLE 和电池缓存。 */
void StatusBar_Create(lv_obj_t *parent);
void StatusBar_Destroy(void);
void StatusBar_SetVisible(uint8_t visible);
void StatusBar_SetBatteryVisible(uint8_t visible);
void StatusBar_SetBleVisible(uint8_t visible);
/* 全屏滚动时暂停后台状态刷新，减少额外脏区。 */
void StatusBar_SetUpdatesPaused(uint8_t paused);
#endif
