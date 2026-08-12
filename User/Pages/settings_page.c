#include "settings_page.h"

#include "device_manager.h"
#include "lcd_init.h"
#include "lvgl.h"

#define SETTINGS_DEFAULT_WORK_BRIGHTNESS     50U
#define SETTINGS_DEFAULT_AMBIENT_BRIGHTNESS  5U

static lv_obj_t *s_work_slider;
static lv_obj_t *s_ambient_slider;
static lv_obj_t *s_work_value;
static lv_obj_t *s_ambient_value;

static void SettingRowClicked(lv_event_t *event)
{
    lv_obj_t *toggle = (lv_obj_t *)lv_event_get_user_data(event);

    if(lv_obj_has_state(toggle, LV_STATE_CHECKED)) {
        lv_obj_clear_state(toggle, LV_STATE_CHECKED);
    }
    else {
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }
    (void)lv_event_send(toggle, LV_EVENT_VALUE_CHANGED, NULL);
}

static void WristSwitchChanged(lv_event_t *event)
{
    lv_obj_t *toggle = lv_event_get_target(event);
    DeviceManager_SetWristWakeEnabled(
        lv_obj_has_state(toggle, LV_STATE_CHECKED) ? 1U : 0U);
}

static void AmbientSwitchChanged(lv_event_t *event)
{
    lv_obj_t *toggle = lv_event_get_target(event);
    DeviceManager_SetAmbientEnabled(
        lv_obj_has_state(toggle, LV_STATE_CHECKED) ? 1U : 0U);
}

static lv_obj_t *CreatePanel(lv_obj_t *parent, lv_coord_t height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 228, height);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x111720U), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    return panel;
}

static void CreateSettingRow(lv_obj_t *parent,
                             const char *title,
                             const char *description,
                             uint8_t checked,
                             lv_event_cb_t callback)
{
    lv_obj_t *panel = CreatePanel(parent, 78);
    lv_obj_t *title_label;
    lv_obj_t *description_label;
    lv_obj_t *toggle;

    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, -2);

    description_label = lv_label_create(panel);
    lv_label_set_text(description_label, description);
    lv_obj_set_width(description_label, 145);
    lv_obj_set_style_text_font(description_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(description_label, lv_color_hex(0x7F8995U), LV_PART_MAIN);
    lv_obj_align(description_label, LV_ALIGN_BOTTOM_LEFT, 0, 2);

    toggle = lv_switch_create(panel);
    lv_obj_set_size(toggle, 64, 36);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_ext_click_area(toggle, 16);
    if(checked != 0U) lv_obj_add_state(toggle, LV_STATE_CHECKED);
    lv_obj_add_event_cb(toggle, callback, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(panel, SettingRowClicked, LV_EVENT_CLICKED, toggle);
}

static void UpdateBrightnessLabels(void)
{
    if(s_work_value != NULL) {
        lv_label_set_text_fmt(s_work_value, "%ld%%",
                              (long)lv_slider_get_value(s_work_slider));
    }
    if(s_ambient_value != NULL) {
        lv_label_set_text_fmt(s_ambient_value, "%ld%%",
                              (long)lv_slider_get_value(s_ambient_slider));
    }
}

static void BrightnessChanged(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);
    int32_t working = lv_slider_get_value(s_work_slider);
    int32_t ambient = lv_slider_get_value(s_ambient_slider);

    if((target == s_work_slider) && (ambient > working)) {
        ambient = working;
        lv_slider_set_value(s_ambient_slider, ambient, LV_ANIM_OFF);
    }
    else if((target == s_ambient_slider) && (ambient > working)) {
        ambient = working;
        lv_slider_set_value(s_ambient_slider, ambient, LV_ANIM_OFF);
    }

    UpdateBrightnessLabels();
    if(target == s_work_slider) {
        /* Live preview is intentionally limited to the working slider. */
        LCD_Set_Light((uint8_t)working);
    }

    if(lv_event_get_code(event) == LV_EVENT_RELEASED) {
        DeviceManager_SetBrightness((uint8_t)working, (uint8_t)ambient);
    }
}

static void CreateBrightnessRow(lv_obj_t *parent,
                                const char *title,
                                uint8_t value,
                                lv_obj_t **slider_out,
                                lv_obj_t **value_out)
{
    lv_obj_t *panel = CreatePanel(parent, 96);
    lv_obj_t *title_label = lv_label_create(panel);
    lv_obj_t *value_label = lv_label_create(panel);
    lv_obj_t *slider = lv_slider_create(panel);

    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, -1);

    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0x55B8FFU), LV_PART_MAIN);
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, -1);

    lv_obj_set_size(slider, 196, 20);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_ext_click_area(slider, 14);
    lv_obj_add_event_cb(slider, BrightnessChanged, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, BrightnessChanged, LV_EVENT_RELEASED, NULL);

    *slider_out = slider;
    *value_out = value_label;
}

static void ResetBrightness(lv_event_t *event)
{
    (void)event;
    lv_slider_set_value(s_work_slider,
                        SETTINGS_DEFAULT_WORK_BRIGHTNESS,
                        LV_ANIM_OFF);
    lv_slider_set_value(s_ambient_slider,
                        SETTINGS_DEFAULT_AMBIENT_BRIGHTNESS,
                        LV_ANIM_OFF);
    UpdateBrightnessLabels();
    LCD_Set_Light(SETTINGS_DEFAULT_WORK_BRIGHTNESS);
    DeviceManager_SetBrightness(SETTINGS_DEFAULT_WORK_BRIGHTNESS,
                                SETTINGS_DEFAULT_AMBIENT_BRIGHTNESS);
}

void SettingsPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *list;
    lv_obj_t *title;
    lv_obj_t *reset_button;
    lv_obj_t *reset_label;
    lv_obj_t *notice;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    list = lv_obj_create(screen);
    lv_obj_set_size(list, 240, 280);
    lv_obj_center(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    title = lv_label_create(list);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);

    CreateSettingRow(list, "Raise to wake", "Use MPU6050 wrist gesture",
                     DeviceManager_GetWristWakeEnabled(), WristSwitchChanged);
    CreateSettingRow(list, "Ambient display", "Off selects STOP mode",
                     DeviceManager_GetAmbientEnabled(), AmbientSwitchChanged);

    CreateBrightnessRow(list, "Working brightness",
                        DeviceManager_GetWorkingBrightness(),
                        &s_work_slider, &s_work_value);
    CreateBrightnessRow(list, "Ambient brightness",
                        DeviceManager_GetAmbientBrightness(),
                        &s_ambient_slider, &s_ambient_value);
    UpdateBrightnessLabels();

    reset_button = lv_btn_create(list);
    lv_obj_set_size(reset_button, 180, 44);
    lv_obj_add_event_cb(reset_button, ResetBrightness, LV_EVENT_CLICKED, NULL);
    reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "Restore 50% / 5%");
    lv_obj_center(reset_label);

    notice = lv_label_create(list);
    lv_label_set_text(notice, "Saved to BL24C02 after release");
    lv_obj_set_style_text_font(notice, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(notice, lv_color_hex(0x68727EU), LV_PART_MAIN);
}

void SettingsPage_Destroy(void)
{
    if((s_work_slider != NULL) && (s_ambient_slider != NULL)) {
        DeviceManager_SetBrightness(
            (uint8_t)lv_slider_get_value(s_work_slider),
            (uint8_t)lv_slider_get_value(s_ambient_slider));
    }
    lv_obj_clean(lv_scr_act());
    s_work_slider = NULL;
    s_ambient_slider = NULL;
    s_work_value = NULL;
    s_ambient_value = NULL;
}
