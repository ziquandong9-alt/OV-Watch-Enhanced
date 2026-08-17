#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* 枚举值是 UI 状态机的唯一页面标识，Create/Destroy 的 switch 必须成对覆盖。 */
    APP_UI_PAGE_WATCH = 0,
    APP_UI_PAGE_MENU,
    APP_UI_PAGE_DATE_SETTING,
    APP_UI_PAGE_TIME_SETTING,
    APP_UI_PAGE_MOTION,
    APP_UI_PAGE_HEART,
    APP_UI_PAGE_ENVIRONMENT,
    APP_UI_PAGE_COMPASS,
    APP_UI_PAGE_CALCULATOR,
    APP_UI_PAGE_STOPWATCH,
    APP_UI_PAGE_NOTIFICATIONS,
    APP_UI_PAGE_BATTERY,
    APP_UI_PAGE_HISTORY,
    APP_UI_PAGE_SETTINGS,
    APP_UI_PAGE_ABOUT
} AppUI_Page_t;

/* 初始化 UI 状态并创建默认表盘；硬件和 DeviceManager 必须先完成初始化。 */
void AppUI_Init(void);
/* 异步提交页面请求，实际切换由下一轮 AppUI_Process() 执行。 */
void AppUI_RequestPage(AppUI_Page_t page);
/* 处理物理 KEY1 的“进入/返回”语义。 */
void AppUI_HandleKey1(void);
/* 报告一次有效用户活动，同时推迟自动息屏。 */
void AppUI_NotifyActivity(void);
/* 在主循环中推进页面、AOD、STOP 和唤醒状态机。 */
void AppUI_Process(void);
AppUI_Page_t AppUI_GetCurrentPage(void);
uint8_t AppUI_IsAmbient(void);
uint8_t AppUI_IsStop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
