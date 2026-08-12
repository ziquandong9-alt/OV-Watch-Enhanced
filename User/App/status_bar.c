#include "status_bar.h"
#include "battery_manager.h"
#include "ble_manager.h"

static lv_obj_t *s_bar;
static lv_obj_t *s_ble;
static lv_obj_t *s_battery;
static lv_timer_t *s_timer;

static void Refresh(lv_timer_t *timer)
{
    const Battery_Data_t *data = BatteryManager_Get();
    if((s_bar == NULL) || (s_ble == NULL) || (s_battery == NULL)) return;
    if((lv_obj_is_valid(s_bar) == false) ||
       (lv_obj_is_valid(s_ble) == false) ||
       (lv_obj_is_valid(s_battery) == false)) {
        s_bar = s_ble = s_battery = NULL;
        if(timer != NULL) lv_timer_pause(timer);
        return;
    }
    lv_label_set_text(s_ble, BLEManager_IsConnected() ? "BLE" : "--");
    lv_obj_set_style_text_color(s_ble,
        BLEManager_IsConnected() ? lv_color_hex(0x45A3FFU) : lv_color_hex(0x59616DU), 0);
    if(data->present == 0U) lv_label_set_text(s_battery, "EXT");
    else lv_label_set_text_fmt(s_battery, data->charging ? "+%u%%" : "%u%%", data->percent);
    lv_obj_set_style_text_color(s_battery,
        data->low ? lv_color_hex(0xFF4D4FU) : lv_color_hex(0xD8DDE5U), 0);
}

void StatusBar_Create(lv_obj_t *parent)
{
    StatusBar_Destroy();
    s_bar = lv_obj_create(parent);
    /* 240 px rounded panel: keep both labels inside the top safe area. */
    lv_obj_set_size(s_bar, 196, 18);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    s_ble = lv_label_create(s_bar);
    lv_obj_set_style_text_font(s_ble, &lv_font_montserrat_14, 0);
    lv_obj_align(s_ble, LV_ALIGN_LEFT_MID, 0, 0);
    s_battery = lv_label_create(s_bar);
    lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(s_battery, LV_ALIGN_RIGHT_MID, 0, 0);
    s_timer = lv_timer_create(Refresh, 2000U, NULL);
    Refresh(NULL);
}

void StatusBar_Destroy(void)
{
    if(s_timer != NULL) { lv_timer_del(s_timer); s_timer = NULL; }
    if((s_bar != NULL) && lv_obj_is_valid(s_bar)) lv_obj_del(s_bar);
    s_bar = s_ble = s_battery = NULL;
}
