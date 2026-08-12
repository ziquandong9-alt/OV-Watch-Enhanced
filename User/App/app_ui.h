#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
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

void AppUI_Init(void);
void AppUI_RequestPage(AppUI_Page_t page);
void AppUI_HandleKey1(void);
void AppUI_NotifyActivity(void);
void AppUI_Process(void);
AppUI_Page_t AppUI_GetCurrentPage(void);
uint8_t AppUI_IsAmbient(void);
uint8_t AppUI_IsStop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
