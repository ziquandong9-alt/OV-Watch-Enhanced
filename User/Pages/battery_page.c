#include "battery_page.h"
#include "battery_manager.h"
#include "lvgl.h"

static lv_obj_t *s_percent;
static lv_obj_t *s_voltage;
static lv_obj_t *s_state;
static lv_obj_t *s_bar;
static lv_timer_t *s_timer;

static void Refresh(lv_timer_t *timer)
{
    /* 页面只读 BatteryManager 的缓存，刷新 UI 不会触发一次新的 ADC 转换。 */
    const Battery_Data_t *data = BatteryManager_Get();
    (void)timer;
    lv_label_set_text_fmt(s_percent, "%u%%", data->percent);
    lv_label_set_text_fmt(s_voltage, "%u.%03u V", data->voltage_mv / 1000U,
                          data->voltage_mv % 1000U);
    lv_label_set_text(s_state, !data->present ? "External/debug power" :
                      (data->charging ? "Charging" :
                      (data->critical ? "Critical - charge now" :
                       (data->low ? "Low battery" : "On battery"))));
    lv_bar_set_value(s_bar, data->percent, LV_ANIM_OFF);
}

void BatteryPage_Create(void)
{
    /* 按“清屏→建静态控件→建定时器→立即首刷”的顺序建立页面。 */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    title = lv_label_create(screen);
    lv_label_set_text(title, "Battery");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    s_percent = lv_label_create(screen);
    lv_obj_set_style_text_font(s_percent, &lv_font_montserrat_20, 0);
    lv_obj_align(s_percent, LV_ALIGN_TOP_MID, 0, 72);
    s_bar = lv_bar_create(screen);
    lv_obj_set_size(s_bar, 190, 18);
    lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x21C55DU), LV_PART_INDICATOR);
    s_voltage = lv_label_create(screen);
    lv_obj_align(s_voltage, LV_ALIGN_CENTER, 0, 48);
    s_state = lv_label_create(screen);
    lv_obj_set_style_text_color(s_state, lv_color_hex(0xAAB2BDU), 0);
    lv_obj_align(s_state, LV_ALIGN_BOTTOM_MID, 0, -38);
    s_timer = lv_timer_create(Refresh, 1000U, NULL);
    BatteryManager_ForceUpdate();
    Refresh(NULL);
}

void BatteryPage_Destroy(void)
{
    /* timer 必须先删除，否则下一次回调会访问 clean 后的 label。 */
    if(s_timer != NULL) { lv_timer_del(s_timer); s_timer = NULL; }
    lv_obj_clean(lv_scr_act());
    s_percent = s_voltage = s_state = s_bar = NULL;
}
