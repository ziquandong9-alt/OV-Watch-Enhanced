#include "app_ui.h"

#include "CST816.h"
#include "calculator_page.h"
#include "battery_page.h"
#include "battery_manager.h"
#include "ble_manager.h"
#include "date_setting_page.h"
#include "device_manager.h"
#include "key.h"
#include "lcd.h"
#include "lcd_init.h"
#include "menu_page.h"
#include "notification_manager.h"
#include "notification_page.h"
#include "history_page.h"
#include "power_manager.h"
#include "sensor_pages.h"
#include "settings_page.h"
#include "stopwatch_page.h"
#include "time_setting_page.h"
#include "about_page.h"
#include "watch_face.h"
#include "status_bar.h"
#include "lvgl.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>

#define APP_UI_IDLE_TIMEOUT_MS       10000U
#define APP_UI_LONG_PRESS_GUARD_MS   700U

static AppUI_Page_t s_current_page = APP_UI_PAGE_WATCH;
static AppUI_Page_t s_requested_page = APP_UI_PAGE_WATCH;
static uint8_t s_page_request_pending;
static uint8_t s_ambient;
static uint8_t s_stop;
static uint8_t s_wrist_peek_active;
static uint32_t s_last_activity_tick;
static uint32_t s_ignore_long_press_until;
static uint32_t s_last_touch_inactive_ms;
static uint32_t s_notification_generation;

static void AppUI_DestroyCurrentPage(void);
static void AppUI_CreatePage(AppUI_Page_t page);
static void AppUI_EnterLowPower(uint8_t force_stop);
static void AppUI_Wake(uint8_t wrist_peek);
static void AppUI_ExitStop(uint8_t wrist_peek);
static void AppUI_ForceWatchPage(void);
static void AppUI_ApplyWatchStatusBar(void);

void AppUI_Init(void)
{
    s_page_request_pending = 0U;
    s_ambient = 0U;
    s_stop = 0U;
    s_wrist_peek_active = 0U;
    s_current_page = APP_UI_PAGE_WATCH;
    s_last_activity_tick = HAL_GetTick();
    s_ignore_long_press_until = 0U;
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    s_notification_generation = NotificationManager_GetGeneration();
    WatchFace_Create();
    StatusBar_Create(lv_scr_act());
    AppUI_ApplyWatchStatusBar();
    s_last_touch_inactive_ms = lv_disp_get_inactive_time(NULL);
}

void AppUI_RequestPage(AppUI_Page_t page)
{
    uint32_t now = HAL_GetTick();

    if((page == APP_UI_PAGE_TIME_SETTING) &&
       ((int32_t)(now - s_ignore_long_press_until) < 0)) {
        return;
    }

    s_requested_page = page;
    s_page_request_pending = 1U;
    AppUI_NotifyActivity();
}

void AppUI_HandleKey1(void)
{
    if(s_ambient != 0U) {
        AppUI_Wake(0U);
        return;
    }

    AppUI_NotifyActivity();

    switch(s_current_page) {
    case APP_UI_PAGE_WATCH:
        AppUI_RequestPage(APP_UI_PAGE_MENU);
        break;

    case APP_UI_PAGE_MENU:
        AppUI_RequestPage(APP_UI_PAGE_WATCH);
        break;

    case APP_UI_PAGE_MOTION:
    case APP_UI_PAGE_HEART:
    case APP_UI_PAGE_ENVIRONMENT:
    case APP_UI_PAGE_COMPASS:
    case APP_UI_PAGE_CALCULATOR:
    case APP_UI_PAGE_STOPWATCH:
    case APP_UI_PAGE_BATTERY:
    case APP_UI_PAGE_HISTORY:
    case APP_UI_PAGE_SETTINGS:
        AppUI_RequestPage(APP_UI_PAGE_MENU);
        break;

    case APP_UI_PAGE_NOTIFICATIONS:
        if(NotificationPage_HandleBack() == 0U) {
            AppUI_RequestPage(APP_UI_PAGE_WATCH);
        }
        break;

    case APP_UI_PAGE_TIME_SETTING:
        AppUI_RequestPage(APP_UI_PAGE_WATCH);
        break;

    case APP_UI_PAGE_DATE_SETTING:
        AppUI_RequestPage(APP_UI_PAGE_MENU);
        break;

    case APP_UI_PAGE_ABOUT:
        AppUI_RequestPage(APP_UI_PAGE_MENU);
        break;

    default:
        AppUI_RequestPage(APP_UI_PAGE_WATCH);
        break;
    }
}

void AppUI_NotifyActivity(void)
{
    s_last_activity_tick = HAL_GetTick();
    s_wrist_peek_active = 0U;
    if(s_ambient != 0U) {
        AppUI_Wake(0U);
    }
}

void AppUI_Process(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t app_inactive_ms;
    uint32_t touch_inactive_ms;
    uint8_t wake_flags;
    uint8_t new_touch_activity;
    uint8_t wrist_raise;
    uint8_t wrist_lower;

    if((s_stop == 0U) && (BatteryManager_TakeCriticalRequest() != 0U)) {
        AppUI_EnterLowPower(1U);
        return;
    }

    if(s_stop != 0U) {
        if(DeviceManager_TakeWristRaiseEvent() != 0U) {
            AppUI_ExitStop(1U);
            return;
        }
        /* A heart-rate window needs 50 ms sampling, so finish it before WFI. */
        if(DeviceManager_CanEnterStop() == 0U) {
            return;
        }

        wake_flags = PowerManager_StopOnce();
        if((wake_flags & POWER_WAKE_TOUCH) != 0U) {
            AppUI_ExitStop(0U);
        }
        else if(((wake_flags & POWER_WAKE_KEY) != 0U) ||
           (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)) {
            Key_IgnoreUntilRelease();
            AppUI_ExitStop(0U);
        }
        else if(((wake_flags & POWER_WAKE_MPU) != 0U) &&
                (DeviceManager_CheckWristAfterStop() != 0U)) {
            AppUI_ExitStop(1U);
        }
        return;
    }

    app_inactive_ms = (uint32_t)(now - s_last_activity_tick);
    touch_inactive_ms = lv_disp_get_inactive_time(NULL);
    new_touch_activity =
        (touch_inactive_ms < s_last_touch_inactive_ms) ? 1U : 0U;
    wrist_raise = DeviceManager_TakeWristRaiseEvent();
    wrist_lower = DeviceManager_TakeWristLowerEvent();

    s_last_touch_inactive_ms = touch_inactive_ms;

    if((s_current_page == APP_UI_PAGE_WATCH) &&
       (NotificationManager_GetGeneration() != s_notification_generation)) {
        s_notification_generation = NotificationManager_GetGeneration();
        WatchFace_RefreshNotificationIndicator();
    }

    if(new_touch_activity != 0U) {
        s_last_activity_tick = now;
        s_wrist_peek_active = 0U;
    }

    /* In ambient mode a new touch wakes the watch, but performs no page action. */
    if((s_ambient != 0U) &&
       ((new_touch_activity != 0U) || (wrist_raise != 0U))) {
        AppUI_Wake((wrist_raise != 0U) &&
                   (new_touch_activity == 0U) ? 1U : 0U);
        return;
    }

    /* A raise-only wake is just a glance: lowering the wrist sleeps now. */
    if((s_ambient == 0U) &&
       (s_wrist_peek_active != 0U) &&
       (wrist_lower != 0U)) {
        AppUI_EnterLowPower(0U);
        return;
    }

    /*
     * Enter ambient mode only when BOTH activity sources have been idle for
     * ten seconds.  Using LVGL's complete inactivity duration avoids losing a
     * touch when an LCD DMA flush delays this function by more than one frame.
     */
    if((s_ambient == 0U) &&
       (s_current_page != APP_UI_PAGE_HEART) &&
       (app_inactive_ms >= APP_UI_IDLE_TIMEOUT_MS) &&
       (touch_inactive_ms >= APP_UI_IDLE_TIMEOUT_MS)) {
        AppUI_EnterLowPower(0U);
        return;
    }

    if(s_page_request_pending != 0U) {
        AppUI_Page_t next_page = s_requested_page;

        s_page_request_pending = 0U;
        if(next_page != s_current_page) {
            AppUI_DestroyCurrentPage();
            AppUI_CreatePage(next_page);
            s_current_page = next_page;
        }
    }
}

AppUI_Page_t AppUI_GetCurrentPage(void)
{
    return s_current_page;
}

uint8_t AppUI_IsAmbient(void)
{
    return s_ambient;
}

uint8_t AppUI_IsStop(void)
{
    return s_stop;
}

static void AppUI_DestroyCurrentPage(void)
{
    StatusBar_Destroy();
    switch(s_current_page) {
    case APP_UI_PAGE_WATCH:
        WatchFace_Destroy();
        break;
    case APP_UI_PAGE_MENU:
        MenuPage_Destroy();
        break;
    case APP_UI_PAGE_DATE_SETTING:
        DateSettingPage_Destroy();
        break;
    case APP_UI_PAGE_TIME_SETTING:
        TimeSettingPage_Destroy();
        break;
    case APP_UI_PAGE_MOTION:
        MotionPage_Destroy();
        break;
    case APP_UI_PAGE_HEART:
        HeartPage_Destroy();
        break;
    case APP_UI_PAGE_ENVIRONMENT:
        EnvironmentPage_Destroy();
        break;
    case APP_UI_PAGE_COMPASS:
        CompassPage_Destroy();
        break;
    case APP_UI_PAGE_CALCULATOR:
        CalculatorPage_Destroy();
        break;
    case APP_UI_PAGE_STOPWATCH:
        StopwatchPage_Destroy();
        break;
    case APP_UI_PAGE_NOTIFICATIONS:
        NotificationPage_Destroy();
        break;
    case APP_UI_PAGE_BATTERY:
        BatteryPage_Destroy();
        break;
    case APP_UI_PAGE_HISTORY:
        HistoryPage_Destroy();
        break;
    case APP_UI_PAGE_SETTINGS:
        SettingsPage_Destroy();
        break;
    case APP_UI_PAGE_ABOUT:
        AboutPage_Destroy();
        break;
    default:
        lv_obj_clean(lv_scr_act());
        break;
    }
}

static void AppUI_CreatePage(AppUI_Page_t page)
{
    switch(page) {
    case APP_UI_PAGE_WATCH:
        WatchFace_Create();
        break;
    case APP_UI_PAGE_MENU:
        MenuPage_Create();
        break;
    case APP_UI_PAGE_DATE_SETTING:
        DateSettingPage_Create();
        break;
    case APP_UI_PAGE_TIME_SETTING:
        TimeSettingPage_Create();
        break;
    case APP_UI_PAGE_MOTION:
        MotionPage_Create();
        break;
    case APP_UI_PAGE_HEART:
        HeartPage_Create();
        break;
    case APP_UI_PAGE_ENVIRONMENT:
        EnvironmentPage_Create();
        break;
    case APP_UI_PAGE_COMPASS:
        CompassPage_Create();
        break;
    case APP_UI_PAGE_CALCULATOR:
        CalculatorPage_Create();
        break;
    case APP_UI_PAGE_STOPWATCH:
        StopwatchPage_Create();
        break;
    case APP_UI_PAGE_NOTIFICATIONS:
        NotificationPage_Create();
        break;
    case APP_UI_PAGE_BATTERY:
        BatteryPage_Create();
        break;
    case APP_UI_PAGE_HISTORY:
        HistoryPage_Create();
        break;
    case APP_UI_PAGE_SETTINGS:
        SettingsPage_Create();
        break;
    case APP_UI_PAGE_ABOUT:
        AboutPage_Create();
        break;
    default:
        WatchFace_Create();
        break;
    }
    StatusBar_Create(lv_scr_act());
    if(page == APP_UI_PAGE_WATCH) {
        AppUI_ApplyWatchStatusBar();
    }
}

static void AppUI_EnterLowPower(uint8_t force_stop)
{
    AppUI_ForceWatchPage();
    Key_IgnoreUntilRelease();

    s_ambient = 1U;
    s_wrist_peek_active = 0U;
    WatchFace_SetAmbientMode(1U);
    StatusBar_SetVisible(0U);
    if((DeviceManager_GetAmbientEnabled() != 0U) && (force_stop == 0U)) {
        LCD_Set_Light(DeviceManager_GetAmbientBrightness());
        PowerManager_SuspendUnusedPeripherals(0U);
    }
    else {
        PowerManager_BeginStopSession();
        s_stop = 1U;
        LCD_Set_Light(0U);
        LCD_WaitForDMA();
        LCD_ST7789_SleepIn();
        PowerManager_SuspendUnusedPeripherals(1U);
    }
}

static void AppUI_Wake(uint8_t wrist_peek)
{
    uint32_t now = HAL_GetTick();

    if(s_stop != 0U) {
        AppUI_ExitStop(wrist_peek);
        return;
    }

    PowerManager_ResumePeripherals();
    s_ambient = 0U;
    s_wrist_peek_active = wrist_peek;
    s_last_activity_tick = now;
    s_ignore_long_press_until = now + APP_UI_LONG_PRESS_GUARD_MS;
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    WatchFace_SetAmbientMode(0U);
    AppUI_ApplyWatchStatusBar();
}

static void AppUI_ExitStop(uint8_t wrist_peek)
{
    uint32_t now = HAL_GetTick();

    /* Keep both the visible screen and the internal navigation state at WATCH. */
    AppUI_ForceWatchPage();
    PowerManager_EndStopSession();
    PowerManager_ResumePeripherals();
    s_stop = 0U;
    s_ambient = 0U;
    s_wrist_peek_active = wrist_peek;
    s_last_activity_tick = now;
    s_ignore_long_press_until = now + APP_UI_LONG_PRESS_GUARD_MS;
    LCD_ST7789_SleepOut();
    WatchFace_SetAmbientMode(0U);
    AppUI_ApplyWatchStatusBar();
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    lv_obj_invalidate(lv_scr_act());
}

static void AppUI_ForceWatchPage(void)
{
    s_page_request_pending = 0U;
    s_requested_page = APP_UI_PAGE_WATCH;

    if(s_current_page != APP_UI_PAGE_WATCH) {
        AppUI_DestroyCurrentPage();
        WatchFace_Create();
        StatusBar_Create(lv_scr_act());
        AppUI_ApplyWatchStatusBar();
    }
    s_current_page = APP_UI_PAGE_WATCH;
}

static void AppUI_ApplyWatchStatusBar(void)
{
    uint8_t selected = WatchFace_GetSelectedIndex();

    if(selected == 2U) {
        StatusBar_SetVisible(0U);
    }
    else {
        StatusBar_SetVisible(1U);
        StatusBar_SetBatteryVisible((selected == 0U) ? 1U : 0U);
    }
}
