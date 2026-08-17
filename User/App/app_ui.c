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

/*
 * 本文件是整套界面的“状态机中枢”，它不负责具体页面怎样画，而负责：
 * 1. 当前页面是谁、下一页准备切到谁；
 * 2. 何时销毁旧页面并创建新页面；
 * 3. 正常显示、息屏显示和 STOP 三种电源状态怎样切换；
 * 4. 按键、触摸、抬腕等输入最终应该触发什么动作。
 *
 * 阅读提示：页面事件只调用 AppUI_RequestPage() 提交请求，真正的页面重建
 * 放在 AppUI_Process() 中完成。这样可避免在 LVGL 正在分发事件时删除事件
 * 所属对象，否则很容易出现野指针或重复回调。
 */

/* 正常亮屏超过 10 秒没有任何活动后进入低功耗流程。 */
#define APP_UI_IDLE_TIMEOUT_MS       10000U
/* 唤醒后的短暂保护时间：防止“唤醒手势”继续被识别成长按设置时间。 */
#define APP_UI_LONG_PRESS_GUARD_MS   700U

/* 当前已经创建并显示的页面。 */
static AppUI_Page_t s_current_page = APP_UI_PAGE_WATCH;
/* 事件回调最近一次请求跳转到的目标页面。 */
static AppUI_Page_t s_requested_page = APP_UI_PAGE_WATCH;
/* 为 1 表示主循环还没有消费本次页面切换请求。 */
static uint8_t s_page_request_pending;
/* 为 1 表示表盘已进入低亮度的息屏显示形态。 */
static uint8_t s_ambient;
/* 为 1 表示 LCD 已休眠、MCU 正在周期性进入 STOP。 */
static uint8_t s_stop;
/* 仅由抬腕唤醒时置位；放下手腕后可以立即重新休眠。 */
static uint8_t s_wrist_peek_active;
/* 应用层最后一次有效操作的 HAL 毫秒时刻，用于计算超时。 */
static uint32_t s_last_activity_tick;
/* 在这个绝对时刻之前，忽略进入“时间设置页”的长按请求。 */
static uint32_t s_ignore_long_press_until;
/* 保存上一轮 LVGL 无操作时长；数值突然变小就说明刚发生了触摸。 */
static uint32_t s_last_touch_inactive_ms;
/* 通知列表的版本号快照；版本变化时才刷新表盘红点，避免每轮重画。 */
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
    /* 先把软件状态统一复位到“正常亮屏的表盘页”。 */
    s_page_request_pending = 0U;
    s_ambient = 0U;
    s_stop = 0U;
    s_wrist_peek_active = 0U;
    s_current_page = APP_UI_PAGE_WATCH;
    s_last_activity_tick = HAL_GetTick(); /* 从开机时刻重新计算空闲时间。 */
    s_ignore_long_press_until = 0U;
    /* 亮度来自 EEPROM 中保存的用户设置，而不是写死在 UI 层。 */
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    s_notification_generation = NotificationManager_GetGeneration();
    /* 页面必须先创建，状态栏后创建；后创建的对象位于更靠上的绘制层。 */
    WatchFace_Create();
    StatusBar_Create(lv_scr_act());
    AppUI_ApplyWatchStatusBar();
    s_last_touch_inactive_ms = lv_disp_get_inactive_time(NULL);
}

void AppUI_RequestPage(AppUI_Page_t page)
{
    uint32_t now = HAL_GetTick();

    /* 用有符号差值比较支持 32 位 tick 回绕；不要直接写 now < deadline。 */
    if((page == APP_UI_PAGE_TIME_SETTING) &&
       ((int32_t)(now - s_ignore_long_press_until) < 0)) {
        return;
    }

    /* 这里只登记请求，不在事件回调栈中直接删除当前 LVGL 页面。 */
    s_requested_page = page;
    s_page_request_pending = 1U;
    AppUI_NotifyActivity();
}

void AppUI_HandleKey1(void)
{
    /* 息屏时第一次按键只负责唤醒，不把同一次按键继续解释为页面跳转。 */
    if(s_ambient != 0U) {
        AppUI_Wake(0U);
        return;
    }

    AppUI_NotifyActivity();

    /* 按键相当于“进入/返回键”，返回目的地取决于页面层级。 */
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
        /* 通知页内部可能还有详情层；先让页面自己消费一次返回操作。 */
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
    /* 所有有效操作都应调用这里，统一推迟自动息屏。 */
    s_last_activity_tick = HAL_GetTick();
    s_wrist_peek_active = 0U;
    if(s_ambient != 0U) {
        AppUI_Wake(0U);
    }
}

void AppUI_Process(void)
{
    /* 该函数由 while(1) 高频调用，是 UI 状态机唯一的推进点。 */
    uint32_t now = HAL_GetTick();
    uint32_t app_inactive_ms;
    uint32_t touch_inactive_ms;
    uint8_t wake_flags;
    uint8_t new_touch_activity;
    uint8_t wrist_raise;
    uint8_t wrist_lower;

    /* 低电量请求是一次性事件；即使用户正在操作也必须优先强制关屏。 */
    if((s_stop == 0U) && (BatteryManager_TakeCriticalRequest() != 0U)) {
        AppUI_EnterLowPower(1U);
        return;
    }

    if(s_stop != 0U) {
        /* STOP 状态下不再执行普通页面逻辑，只处理允许唤醒系统的事件。 */
        if(DeviceManager_TakeWristRaiseEvent() != 0U) {
            AppUI_ExitStop(1U);
            return;
        }
        /* 心率测量窗口要求约 50 ms 的连续采样，窗口未结束时不能执行 WFI。 */
        if(DeviceManager_CanEnterStop() == 0U) {
            return;
        }

        /* 每次调用只睡眠一次；中断唤醒后返回唤醒源位图，再决定是否真正亮屏。 */
        wake_flags = PowerManager_StopOnce();
        if((wake_flags & POWER_WAKE_TOUCH) != 0U) {
            AppUI_ExitStop(0U);
        }
        else if(((wake_flags & POWER_WAKE_KEY) != 0U) ||
           (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)) {
            /* 防止唤醒用的按下状态在下一轮又被 Key_Proc() 报告一次。 */
            Key_IgnoreUntilRelease();
            AppUI_ExitStop(0U);
        }
        else if(((wake_flags & POWER_WAKE_MPU) != 0U) &&
                (DeviceManager_CheckWristAfterStop() != 0U)) {
            AppUI_ExitStop(1U);
        }
        return;
    }

    /* 无符号减法天然支持 HAL tick 溢出后的时间差计算。 */
    app_inactive_ms = (uint32_t)(now - s_last_activity_tick);
    touch_inactive_ms = lv_disp_get_inactive_time(NULL);
    /* LVGL 一旦收到输入会把 inactive_time 清零，因此新值小于旧值即代表新触摸。 */
    new_touch_activity =
        (touch_inactive_ms < s_last_touch_inactive_ms) ? 1U : 0U;
    /* Take 接口会同时清除事件，确保一个动作只消费一次。 */
    wrist_raise = DeviceManager_TakeWristRaiseEvent();
    wrist_lower = DeviceManager_TakeWristLowerEvent();

    s_last_touch_inactive_ms = touch_inactive_ms;

    if((s_current_page == APP_UI_PAGE_WATCH) &&
       (NotificationManager_GetGeneration() != s_notification_generation)) {
        s_notification_generation = NotificationManager_GetGeneration();
        /* 只有通知代数变化才更新红点，避免主循环持续使对象失效。 */
        WatchFace_RefreshNotificationIndicator();
    }

    if(new_touch_activity != 0U) {
        s_last_activity_tick = now;
        s_wrist_peek_active = 0U;
    }

    /* 息屏时新触摸/抬腕只唤醒，不执行这次触摸原本命中的页面动作。 */
    if((s_ambient != 0U) &&
       ((new_touch_activity != 0U) || (wrist_raise != 0U))) {
        AppUI_Wake((wrist_raise != 0U) &&
                   (new_touch_activity == 0U) ? 1U : 0U);
        return;
    }

    /* 纯抬腕唤醒属于“看一眼”：检测到落腕便立即回到低功耗。 */
    if((s_ambient == 0U) &&
       (s_wrist_peek_active != 0U) &&
       (wrist_lower != 0U)) {
        AppUI_EnterLowPower(0U);
        return;
    }

    /*
     * 应用活动时间和 LVGL 触摸空闲时间必须同时达到 10 秒才息屏。
     * 两个条件缺一不可：LCD DMA 可能让本函数晚执行一帧，只看应用计时会把
     * 刚发生但尚未在这里消费的触摸漏掉。心率页测量期间则明确禁止自动息屏。
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

        /* 先清标志再创建页面；创建过程即使又发出请求，也不会被本次误清除。 */
        s_page_request_pending = 0U;
        if(next_page != s_current_page) {
            /* 严格遵守先销毁后创建，避免两个页面的定时器同时操作同一屏幕。 */
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
    /* 只读查询，供页面决定是否允许交互或显示高复杂度内容。 */
    return s_ambient;
}

uint8_t AppUI_IsStop(void)
{
    /* 主循环据此跳过已经关闭时钟的 LCD/LVGL 服务。 */
    return s_stop;
}

static void AppUI_DestroyCurrentPage(void)
{
    /* 状态栏由所有页面共享，但其对象仍属于当前屏幕，所以应先停止其定时器。 */
    StatusBar_Destroy();
    /* 每个页面负责删除自己创建的 LVGL timer/对象并清空静态指针。 */
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
        /* 防御性兜底：未知状态至少清空屏幕，避免残留对象继续接收事件。 */
        lv_obj_clean(lv_scr_act());
        break;
    }
}

static void AppUI_CreatePage(AppUI_Page_t page)
{
    /* 根据页面枚举调用唯一的 Create 入口，建立统一的页面生命周期。 */
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
    /* 状态栏最后创建，保证它处于页面内容上层。 */
    StatusBar_Create(lv_scr_act());
    if(page == APP_UI_PAGE_WATCH) {
        AppUI_ApplyWatchStatusBar();
    }
}

static void AppUI_EnterLowPower(uint8_t force_stop)
{
    /* 无论从哪一页休眠，醒来时都统一回到用户最后选择的表盘。 */
    AppUI_ForceWatchPage();
    /* 若按键仍按着，禁止其释放/抖动形成新的页面事件。 */
    Key_IgnoreUntilRelease();

    s_ambient = 1U;
    s_wrist_peek_active = 0U;
    /* 先把表盘切换成 AOD 内容，再降低背光或让 LCD Sleep In。 */
    WatchFace_SetAmbientMode(1U);
    StatusBar_SetVisible(0U);
    if((DeviceManager_GetAmbientEnabled() != 0U) && (force_stop == 0U)) {
        /* 用户启用 AOD：屏幕继续工作，只关闭暂时不用的外设。 */
        LCD_Set_Light(DeviceManager_GetAmbientBrightness());
        PowerManager_SuspendUnusedPeripherals(0U);
    }
    else {
        /* 强制 STOP 或未启用 AOD：保存会话状态并彻底关闭显示链路。 */
        PowerManager_BeginStopSession();
        s_stop = 1U;
        LCD_Set_Light(0U);
        /* 必须等待最后一批 SPI DMA 像素发送完，再向 LCD 发休眠命令。 */
        LCD_WaitForDMA();
        LCD_ST7789_SleepIn();
        PowerManager_SuspendUnusedPeripherals(1U);
    }
}

static void AppUI_Wake(uint8_t wrist_peek)
{
    uint32_t now = HAL_GetTick();

    if(s_stop != 0U) {
        /* STOP 唤醒还需要重新初始化外设，交给更完整的 ExitStop 路径。 */
        AppUI_ExitStop(wrist_peek);
        return;
    }

    /* AOD 唤醒只需恢复被暂停的外设，不需要 LCD Sleep Out。 */
    PowerManager_ResumePeripherals();
    s_ambient = 0U;
    s_wrist_peek_active = wrist_peek;
    s_last_activity_tick = now;
    /* 避免抬腕/触摸唤醒动作在恢复后的页面里被接续成长按。 */
    s_ignore_long_press_until = now + APP_UI_LONG_PRESS_GUARD_MS;
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    WatchFace_SetAmbientMode(0U);
    AppUI_ApplyWatchStatusBar();
}

static void AppUI_ExitStop(uint8_t wrist_peek)
{
    uint32_t now = HAL_GetTick();

    /* 同时把可见内容和内部导航状态固定到表盘，避免醒后状态与画面不一致。 */
    AppUI_ForceWatchPage();
    PowerManager_EndStopSession();
    PowerManager_ResumePeripherals();
    s_stop = 0U;
    s_ambient = 0U;
    s_wrist_peek_active = wrist_peek;
    s_last_activity_tick = now;
    s_ignore_long_press_until = now + APP_UI_LONG_PRESS_GUARD_MS;
    /* LCD 控制器先退出 Sleep，再恢复表盘模式和背光。 */
    LCD_ST7789_SleepOut();
    WatchFace_SetAmbientMode(0U);
    AppUI_ApplyWatchStatusBar();
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());
    /* LCD 睡眠期间显存内容不可信，强制 LVGL 重画当前整个屏幕。 */
    lv_obj_invalidate(lv_scr_act());
}

static void AppUI_ForceWatchPage(void)
{
    /* 丢弃尚未执行的跳转，防止刚休眠又被旧请求切走。 */
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

    /* 第三个表盘设计为纯数字，不显示任何状态栏。 */
    if(selected == 2U) {
        StatusBar_SetVisible(0U);
    }
    else {
        StatusBar_SetVisible(1U);
        /* 第二表盘自带电量组件；只有第一个表盘需要右上角电池。 */
        StatusBar_SetBatteryVisible((selected == 0U) ? 1U : 0U);
    }
}
