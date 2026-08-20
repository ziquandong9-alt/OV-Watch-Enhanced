#include "sensor_pages.h"

#include "app_ui.h"
#include "device_manager.h"
#include "lvgl.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>

static lv_timer_t *s_page_timer;
static lv_obj_t *s_value_label;
static lv_obj_t *s_detail_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_motion_goal_label;
static lv_obj_t *s_motion_progress_bar;
static lv_obj_t *s_motion_goal_slider;
static lv_obj_t *s_motion_goal_value_label;
static lv_obj_t *s_compass_meter;
static lv_meter_indicator_t *s_compass_needle;
static lv_obj_t *s_compass_heading_label;

static lv_obj_t *CreateBase(const char *title, uint32_t accent)
{
    /* 四个传感器页共享标题、数值、详情和状态布局，统一在这里创建。 */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title_label;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title_label = lv_label_create(screen);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_hex(accent), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 18);

    s_value_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_value_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_value_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_value_label, LV_ALIGN_CENTER, 0, -25);

    s_detail_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_detail_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_detail_label, lv_color_hex(0xB6BDC6U), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_detail_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_detail_label, LV_ALIGN_CENTER, 0, 35);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x737D89U), LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -18);
    return screen;
}

static uint32_t RemainingMinutes(uint32_t deadline)
{
    /* deadline 已到时返回 0，否则向上取整为剩余分钟数供用户理解。 */
    int32_t remaining = (int32_t)(deadline - HAL_GetTick());
    if(remaining <= 0) return 0U;
    return ((uint32_t)remaining + 59999U) / 60000U;
}

static void DeletePageTimer(void)
{
    /* 页面切换前停止唯一的刷新 timer，防止回调落到下一页面对象上。 */
    if(s_page_timer != NULL) {
        lv_timer_del(s_page_timer);
        s_page_timer = NULL;
    }
    lv_obj_clean(lv_scr_act());
    s_value_label = NULL;
    s_detail_label = NULL;
    s_status_label = NULL;
    s_motion_goal_label = NULL;
    s_motion_progress_bar = NULL;
    s_motion_goal_slider = NULL;
    s_motion_goal_value_label = NULL;
}

static void MotionUpdate(lv_timer_t *timer)
{
    /* 页面只读取后台持续维护的计步缓存，并计算今日目标完成比例。 */
    const Device_MotionData_t *data;
    uint32_t goal;
    uint32_t percent;
    uint32_t remaining;
    (void)timer;
    data = DeviceManager_GetMotion();
    goal = DeviceManager_GetMotionGoal();
    percent = (data->steps_today >= goal) ? 100UL :
              (data->steps_today * 100UL) / goal;
    remaining = (data->steps_today >= goal) ? 0UL :
                goal - data->steps_today;

    lv_label_set_text_fmt(s_value_label, "%lu",
                          (unsigned long)data->steps_today);
    lv_label_set_text(s_detail_label, "STEPS TODAY");
    lv_label_set_text_fmt(s_motion_goal_label, "%lu / %lu STEPS",
                          (unsigned long)data->steps_today,
                          (unsigned long)goal);
    lv_bar_set_value(s_motion_progress_bar, (int32_t)percent, LV_ANIM_ON);
    if(remaining == 0UL) {
        lv_label_set_text(s_status_label, "TODAY'S GOAL COMPLETE!");
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_hex(0x75E6A4U), LV_PART_MAIN);
    }
    else if(data->connected == 0U) {
        lv_label_set_text(s_status_label, "MOTION SENSOR NOT FOUND");
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_hex(0xEF8C8CU), LV_PART_MAIN);
    }
    else {
        lv_label_set_text_fmt(s_status_label, "%lu STEPS TO GO",
                              (unsigned long)remaining);
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_hex(0x919AA6U), LV_PART_MAIN);
    }
}

static void MotionGoalOpenEvent(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_get_act();
    (void)event;
    if(indev != NULL) lv_indev_wait_release(indev);
    AppUI_RequestPage(APP_UI_PAGE_MOTION_GOAL);
}

void MotionPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *button;
    lv_obj_t *button_label;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "ACTIVITY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xDC80E6U), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    s_value_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_value_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_value_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_value_label, LV_ALIGN_TOP_MID, 0, 52);
    lv_label_set_text(s_value_label, "0");

    s_detail_label = lv_label_create(screen);
    lv_label_set_text(s_detail_label, "STEPS TODAY");
    lv_obj_set_style_text_font(s_detail_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_detail_label, lv_color_hex(0x9DA6B2U), LV_PART_MAIN);
    lv_obj_align(s_detail_label, LV_ALIGN_TOP_MID, 0, 82);

    s_motion_goal_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_motion_goal_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_motion_goal_label, lv_color_hex(0xD9DDE4U), LV_PART_MAIN);
    lv_obj_align(s_motion_goal_label, LV_ALIGN_TOP_MID, 0, 108);

    s_motion_progress_bar = lv_bar_create(screen);
    lv_obj_set_size(s_motion_progress_bar, 190, 12);
    lv_obj_align(s_motion_progress_bar, LV_ALIGN_TOP_MID, 0, 134);
    lv_bar_set_range(s_motion_progress_bar, 0, 100);
    lv_obj_set_style_radius(s_motion_progress_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_motion_progress_bar, lv_color_hex(0x242A33U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_motion_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_motion_progress_bar, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_motion_progress_bar, lv_color_hex(0xC36BE0U), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_motion_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 158);

    button = lv_btn_create(screen);
    lv_obj_set_size(button, 184, 44);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(button, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x7F46A6U), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, MotionGoalOpenEvent, LV_EVENT_CLICKED, NULL);
    button_label = lv_label_create(button);
    lv_label_set_text(button_label, "SET DAILY GOAL");
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(button_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(button_label);

    DeviceManager_MotionOpen();
    MotionUpdate(NULL);
    s_page_timer = lv_timer_create(MotionUpdate, 500U, NULL);
}

void MotionPage_Destroy(void)
{
    /* 先停页面 timer，再让设备层恢复低功耗计步模式。 */
    DeviceManager_MotionClose();
    DeletePageTimer();
}

static void MotionGoalUpdateValueLabel(void)
{
    uint32_t goal = (uint32_t)lv_slider_get_value(s_motion_goal_slider) * 500UL;
    lv_label_set_text_fmt(s_motion_goal_value_label, "%lu STEPS",
                          (unsigned long)goal);
}

static void MotionGoalSliderChanged(lv_event_t *event)
{
    (void)event;
    MotionGoalUpdateValueLabel();
}

static void MotionGoalSaveEvent(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_get_act();
    (void)event;
    DeviceManager_SetMotionGoal(
        (uint32_t)lv_slider_get_value(s_motion_goal_slider) * 500UL);
    if(indev != NULL) lv_indev_wait_release(indev);
    AppUI_RequestPage(APP_UI_PAGE_MOTION);
}

void MotionGoalPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *hint;
    lv_obj_t *minimum;
    lv_obj_t *maximum;
    lv_obj_t *button;
    lv_obj_t *button_label;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "DAILY GOAL");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xDC80E6U), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    hint = lv_label_create(screen);
    lv_label_set_text(hint, "Choose your target");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x919AA6U), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 60);

    s_motion_goal_value_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_motion_goal_value_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_motion_goal_value_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_motion_goal_value_label, LV_ALIGN_TOP_MID, 0, 88);

    s_motion_goal_slider = lv_slider_create(screen);
    lv_obj_set_size(s_motion_goal_slider, 190, 16);
    lv_obj_align(s_motion_goal_slider, LV_ALIGN_TOP_MID, 0, 138);
    lv_slider_set_range(s_motion_goal_slider, 2, 60);
    lv_slider_set_value(s_motion_goal_slider,
                        (int32_t)(DeviceManager_GetMotionGoal() / 500UL),
                        LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_motion_goal_slider, lv_color_hex(0x292F39U), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_motion_goal_slider, lv_color_hex(0xC36BE0U), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_motion_goal_slider, lv_color_hex(0xF0C6FAU), LV_PART_KNOB);
    lv_obj_add_event_cb(s_motion_goal_slider, MotionGoalSliderChanged,
                        LV_EVENT_VALUE_CHANGED, NULL);

    minimum = lv_label_create(screen);
    lv_label_set_text(minimum, "1,000");
    lv_obj_set_style_text_color(minimum, lv_color_hex(0x737D89U), LV_PART_MAIN);
    lv_obj_align(minimum, LV_ALIGN_TOP_LEFT, 25, 160);
    maximum = lv_label_create(screen);
    lv_label_set_text(maximum, "30,000");
    lv_obj_set_style_text_color(maximum, lv_color_hex(0x737D89U), LV_PART_MAIN);
    lv_obj_align(maximum, LV_ALIGN_TOP_RIGHT, -25, 160);

    button = lv_btn_create(screen);
    lv_obj_set_size(button, 184, 46);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_set_style_radius(button, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x7F46A6U), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, MotionGoalSaveEvent, LV_EVENT_CLICKED, NULL);
    button_label = lv_label_create(button);
    lv_label_set_text(button_label, "SAVE GOAL");
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(button_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(button_label);
    MotionGoalUpdateValueLabel();
}

void MotionGoalPage_Destroy(void)
{
    DeletePageTimer();
}

static void HeartUpdate(lv_timer_t *timer)
{
    /* 测量中显示动态 BPM/raw，结束后显示下次自动测量倒计时。 */
    const Device_HeartData_t *data = DeviceManager_GetHeart();
    (void)timer;
    if(data->connected == 0U) {
        lv_label_set_text(s_value_label, "NOT FOUND");
        lv_label_set_text(s_detail_label, "Check EM7028 on PB13/PB14");
        return;
    }
    if(data->bpm == 0U) lv_label_set_text(s_value_label, "-- BPM");
    else lv_label_set_text_fmt(s_value_label, "%u BPM", (unsigned int)data->bpm);
    lv_label_set_text_fmt(s_detail_label, "Raw: %u", (unsigned int)data->raw);
    if(data->measuring != 0U) lv_label_set_text(s_status_label, "Measuring for 12 seconds...");
    else lv_label_set_text_fmt(s_status_label, "Next automatic run: %lu min",
                               (unsigned long)RemainingMinutes(data->next_update_ms));
}

void HeartPage_Create(void)
{
    /* 进入页面即开启新的测量窗口，不沿用旧峰值检测状态。 */
    (void)CreateBase("HEART RATE", 0xFF5050U);
    DeviceManager_StartHeartMeasurement();
    HeartUpdate(NULL);
    s_page_timer = lv_timer_create(HeartUpdate, 250U, NULL);
}

void HeartPage_Destroy(void)
{
    /* 离开页面提前关闭心率 LED/AFE，避免后台持续耗电。 */
    DeviceManager_StopHeartMeasurement();
    DeletePageTimer();
}

static void EnvironmentUpdate(lv_timer_t *timer)
{
    /* 页面 timer 只显示缓存；AHT21 的异步转换由 DeviceManager_Process 推进。 */
    const Device_EnvironmentData_t *data = DeviceManager_GetEnvironment();
    int32_t temperature_x10;
    uint32_t humidity_x10;
    (void)timer;
    if(data->connected == 0U) {
        lv_label_set_text(s_value_label, "NOT FOUND");
        lv_label_set_text(s_detail_label, "Check AHT21 on PB13/PB14");
        return;
    }
    temperature_x10 = (int32_t)(data->temperature * 10.0f);
    humidity_x10 = (uint32_t)(data->humidity * 10.0f);
    lv_label_set_text_fmt(s_value_label, "%d.%u C",
                          (int)(temperature_x10 / 10),
                          (unsigned int)((temperature_x10 < 0 ?
                          -temperature_x10 : temperature_x10) % 10));
    lv_label_set_text_fmt(s_detail_label, "Humidity: %lu.%lu %%",
                          (unsigned long)(humidity_x10 / 10U),
                          (unsigned long)(humidity_x10 % 10U));
    lv_label_set_text(s_status_label, "Live sample: once per second");
}

void EnvironmentPage_Create(void)
{
    /* Open 把后台 20 分钟周期临时提升到约 1 秒。 */
    (void)CreateBase("ENVIRONMENT", 0x4FCB75U);
    DeviceManager_EnvironmentOpen();
    EnvironmentUpdate(NULL);
    s_page_timer = lv_timer_create(EnvironmentUpdate, 250U, NULL);
}

void EnvironmentPage_Destroy(void)
{
    /* 关闭实时模式后，环境采样恢复为后台 20 分钟周期。 */
    DeviceManager_EnvironmentClose();
    DeletePageTimer();
}

static const char *CompassCardinal(uint16_t degree)
{
    /* 将 0~359° 分成八个 45° 方位；边界偏移后实现就近取整。 */
    static const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return directions[((degree + 22U) / 45U) & 7U];
}

static void CompassDirectionLabel(lv_obj_t *parent, const char *text,
                                  lv_align_t align, int16_t x, int16_t y,
                                  uint32_t color)
{
    /* 在表盘四周创建 N/E/S/W 方位文字，parent 负责其生命周期。 */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
}

static void CompassUpdate(lv_timer_t *timer)
{
    /* 更新设备缓存后同步旋转 meter 指针并更新角度/方位文字。 */
    const Device_CompassData_t *data;
    (void)timer;
    DeviceManager_UpdateCompass();
    data = DeviceManager_GetCompass();
    if(data->connected == 0U) {
        lv_label_set_text(s_compass_heading_label, "NOT FOUND");
        return;
    }
    lv_label_set_text_fmt(s_compass_heading_label, "%s  %u deg",
                          CompassCardinal(data->direction_deg),
                          (unsigned int)data->direction_deg);
    /* Fixed dial follows the watch; the red needle points to magnetic north. */
    lv_meter_set_indicator_value(s_compass_meter, s_compass_needle,
                                 (int32_t)((360U - data->direction_deg) % 360U));
}

void CompassPage_Create(void)
{
    /* 指南针使用 LVGL meter 绘制刻度，needle 指示实时磁方位。 */
    lv_obj_t *screen = lv_scr_act();
    lv_meter_scale_t *scale;
    lv_obj_t *center;
    lv_obj_t *status;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_compass_meter = lv_meter_create(screen);
    lv_obj_set_size(s_compass_meter, 214, 214);
    lv_obj_align(s_compass_meter, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_color(s_compass_meter, lv_color_hex(0x0B1016U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_compass_meter, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_compass_meter, lv_color_hex(0x34404DU), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_compass_meter, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_compass_meter, 13, LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_compass_meter, LV_OPA_TRANSP, LV_PART_TICKS);

    scale = lv_meter_add_scale(s_compass_meter);
    lv_meter_set_scale_ticks(s_compass_meter, scale, 72U, 1U, 7U,
                             lv_color_hex(0x647180U));
    lv_meter_set_scale_major_ticks(s_compass_meter, scale, 18U, 3U, 13U,
                                   lv_color_hex(0xDCE4ECU), 0U);
    lv_meter_set_scale_range(s_compass_meter, scale, 0, 359, 360U, 270U);
    s_compass_needle = lv_meter_add_needle_line(s_compass_meter, scale, 4U,
                                                lv_color_hex(0xFF4D4DU), -18);

    CompassDirectionLabel(s_compass_meter, "N", LV_ALIGN_TOP_MID, 0, 19, 0xFF5252U);
    CompassDirectionLabel(s_compass_meter, "E", LV_ALIGN_RIGHT_MID, -19, 0, 0xE4EAF0U);
    CompassDirectionLabel(s_compass_meter, "S", LV_ALIGN_BOTTOM_MID, 0, -19, 0xE4EAF0U);
    CompassDirectionLabel(s_compass_meter, "W", LV_ALIGN_LEFT_MID, 19, 0, 0xE4EAF0U);

    center = lv_obj_create(s_compass_meter);
    lv_obj_set_size(center, 14, 14);
    lv_obj_center(center);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(center, lv_color_hex(0xF2F5F8U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(center, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(center, lv_color_hex(0xFF4D4DU), LV_PART_MAIN);

    s_compass_heading_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_compass_heading_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_compass_heading_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_compass_heading_label, LV_ALIGN_BOTTOM_MID, 0, -29);

    status = lv_label_create(screen);
    lv_label_set_text(status, "MAGNETIC NORTH");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(status, lv_color_hex(0x687482U), LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -8);

    DeviceManager_CompassOpen();
    CompassUpdate(NULL);
    s_page_timer = lv_timer_create(CompassUpdate, 300U, NULL);
}

void CompassPage_Destroy(void)
{
    /* 关闭 LSM303 并清空 meter/indicator 指针；indicator 随 meter 一起释放。 */
    DeviceManager_CompassClose();
    DeletePageTimer();
    s_compass_meter = NULL;
    s_compass_needle = NULL;
    s_compass_heading_label = NULL;
}
