#ifndef WATCH_FACE_H
#define WATCH_FACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 创建三个表盘页面并启动当前表盘所需的定时器。
 *  调用前必须完成 LVGL、显示端口和 RTC 初始化。 */
void WatchFace_Create(void);

/** 停止全部更新定时器，并释放表盘拥有的 LVGL 对象。 */
void WatchFace_Destroy(void);

/** 在正常动画模式与按分钟更新的低功耗 AOD 模式之间切换。 */
void WatchFace_SetAmbientMode(uint8_t enabled);

/** 根据通知数量刷新三个表盘顶部的红点。 */
void WatchFace_RefreshNotificationIndicator(void);

/** 返回当前表盘：0=机械，1=信息，2=果冻数字。 */
uint8_t WatchFace_GetSelectedIndex(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_FACE_H */
