#include "control_center_page.h"

#include "app_ui.h"
#include "device_manager.h"
#include "lcd_init.h"
#include "lvgl.h"
#include "power_manager.h"

#include <stdint.h>

static lv_obj_t *s_root;
static lv_obj_t *s_brightness_slider;
static lv_obj_t *s_brightness_value;
static lv_obj_t *s_flashlight_layer;
static lv_obj_t *s_power_msgbox;
static uint8_t s_preview_brightness;
static uint8_t s_flashlight_active;

static void ControlCenter_SetY(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, (lv_coord_t)value);
}

static void ControlCenter_AddButtonLabel(lv_obj_t *button,
                                         const char *symbol,
                                         const char *name)
{
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text_fmt(label, "%s\n%s", symbol, name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
}

static void ControlCenter_FlashlightEvent(lv_event_t *event)
{
    lv_obj_t *screen;
    (void)event;

    if(s_flashlight_active != 0U) return;
    s_flashlight_active = 1U;
    LCD_Set_Light(100U);

    /* 白色层直接覆盖整个活动屏幕，也覆盖触摸返回键：手电筒只允许实体键退出。 */
    screen = lv_scr_act();
    s_flashlight_layer = lv_obj_create(screen);
    lv_obj_set_pos(s_flashlight_layer, 0, 0);
    lv_obj_set_size(s_flashlight_layer, LCD_W, LCD_H);
    lv_obj_clear_flag(s_flashlight_layer,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_flashlight_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_flashlight_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_flashlight_layer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_flashlight_layer, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_flashlight_layer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_move_foreground(s_flashlight_layer);
}

static void ControlCenter_BrightnessEvent(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    uint8_t ambient;

    if(s_brightness_slider == NULL) return;
    s_preview_brightness = (uint8_t)lv_slider_get_value(s_brightness_slider);
    if(s_brightness_value != NULL) {
        lv_label_set_text_fmt(s_brightness_value,
                              "%u%%",
                              (unsigned int)s_preview_brightness);
    }
    /* 拖动过程只预览背光，不写 EEPROM。 */
    LCD_Set_Light(s_preview_brightness);

    if(code == LV_EVENT_RELEASED) {
        ambient = DeviceManager_GetAmbientBrightness();
        if(ambient > s_preview_brightness) ambient = s_preview_brightness;
        DeviceManager_SetBrightness(s_preview_brightness, ambient);
    }
}

static void ControlCenter_PowerCancelEvent(lv_event_t *event)
{
    (void)event;
    if((s_power_msgbox != NULL) && lv_obj_is_valid(s_power_msgbox)) {
        lv_obj_del_async(s_power_msgbox);
        s_power_msgbox = NULL;
    }
}

static void ControlCenter_PowerConfirmEvent(lv_event_t *event)
{
    (void)event;
    /* 先保存时间和当天步数，再释放电源锁存。 */
    DeviceManager_SaveDateTimeNow();
    PowerManager_Shutdown();
}

static void ControlCenter_PowerEvent(lv_event_t *event)
{
    lv_obj_t *card;
    lv_obj_t *label;
    lv_obj_t *cancel_button;
    lv_obj_t *confirm_button;
    (void)event;

    if((s_power_msgbox != NULL) && lv_obj_is_valid(s_power_msgbox)) return;
    /* LV_USE_MSGBOX 在本工程中关闭，用普通对象构成更轻量的模态确认框。 */
    s_power_msgbox = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(s_power_msgbox, 0, 0);
    lv_obj_set_size(s_power_msgbox, LCD_W, LCD_H);
    lv_obj_clear_flag(s_power_msgbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_power_msgbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_power_msgbox, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_power_msgbox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_power_msgbox, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_power_msgbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_power_msgbox, 190, LV_PART_MAIN);
    lv_obj_move_foreground(s_power_msgbox);

    card = lv_obj_create(s_power_msgbox);
    lv_obj_set_size(card, 210, 150);
    lv_obj_center(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 20, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x48515DU), LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x171D25U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);

    label = lv_label_create(card);
    lv_label_set_text(label, "POWER OFF?\nTurn off the watch?");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 18);

    cancel_button = lv_btn_create(card);
    lv_obj_set_size(cancel_button, 78, 42);
    lv_obj_align(cancel_button, LV_ALIGN_BOTTOM_LEFT, 8, -10);
    lv_obj_set_style_radius(cancel_button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cancel_button, lv_color_hex(0x3B4551U), LV_PART_MAIN);
    label = lv_label_create(cancel_button);
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);
    lv_obj_add_event_cb(cancel_button,
                        ControlCenter_PowerCancelEvent,
                        LV_EVENT_CLICKED,
                        NULL);

    confirm_button = lv_btn_create(card);
    lv_obj_set_size(confirm_button, 92, 42);
    lv_obj_align(confirm_button, LV_ALIGN_BOTTOM_RIGHT, -8, -10);
    lv_obj_set_style_radius(confirm_button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(confirm_button, lv_color_hex(0xB72C3AU), LV_PART_MAIN);
    label = lv_label_create(confirm_button);
    lv_label_set_text(label, "Power off");
    lv_obj_center(label);
    lv_obj_add_event_cb(confirm_button,
                        ControlCenter_PowerConfirmEvent,
                        LV_EVENT_CLICKED,
                        NULL);
}

void ControlCenterPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *flash_button;
    lv_obj_t *power_button;
    lv_obj_t *brightness_label;
    lv_anim_t animation;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_flashlight_active = 0U;
    s_flashlight_layer = NULL;
    s_power_msgbox = NULL;
    s_preview_brightness = DeviceManager_GetWorkingBrightness();

    s_root = lv_obj_create(screen);
    lv_obj_set_pos(s_root, 0, LCD_H);
    lv_obj_set_size(s_root, LCD_W, LCD_H);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_root, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x10151CU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(s_root);
    lv_label_set_text(title, "CONTROL CENTER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE9EEF5U), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    flash_button = lv_btn_create(s_root);
    lv_obj_set_size(flash_button, 88, 72);
    lv_obj_align(flash_button, LV_ALIGN_CENTER, -52, -45);
    lv_obj_set_style_radius(flash_button, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(flash_button, lv_color_hex(0xE9EEF5U), LV_PART_MAIN);
    lv_obj_set_style_text_color(flash_button, lv_color_hex(0x10151CU), LV_PART_MAIN);
    ControlCenter_AddButtonLabel(flash_button, LV_SYMBOL_CHARGE, "FLASH");
    lv_obj_add_event_cb(flash_button,
                        ControlCenter_FlashlightEvent,
                        LV_EVENT_CLICKED,
                        NULL);

    power_button = lv_btn_create(s_root);
    lv_obj_set_size(power_button, 88, 72);
    lv_obj_align(power_button, LV_ALIGN_CENTER, 52, -45);
    lv_obj_set_style_radius(power_button, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(power_button, lv_color_hex(0x7E2730U), LV_PART_MAIN);
    ControlCenter_AddButtonLabel(power_button, LV_SYMBOL_POWER, "POWER");
    lv_obj_add_event_cb(power_button,
                        ControlCenter_PowerEvent,
                        LV_EVENT_CLICKED,
                        NULL);

    brightness_label = lv_label_create(s_root);
    lv_label_set_text(brightness_label, LV_SYMBOL_EYE_OPEN "  BRIGHTNESS");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xAEB8C5U), LV_PART_MAIN);
    lv_obj_align(brightness_label, LV_ALIGN_CENTER, -26, 45);

    s_brightness_value = lv_label_create(s_root);
    lv_label_set_text_fmt(s_brightness_value,
                          "%u%%",
                          (unsigned int)s_preview_brightness);
    lv_obj_set_style_text_font(s_brightness_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_brightness_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_brightness_value, LV_ALIGN_CENTER, 83, 45);

    s_brightness_slider = lv_slider_create(s_root);
    lv_obj_set_size(s_brightness_slider, 190, 18);
    lv_obj_align(s_brightness_slider, LV_ALIGN_CENTER, 0, 82);
    lv_slider_set_range(s_brightness_slider, 1, 100);
    lv_slider_set_value(s_brightness_slider, s_preview_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0x303945U), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_hex(0xF2C94CU), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_brightness_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_add_event_cb(s_brightness_slider,
                        ControlCenter_BrightnessEvent,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);
    lv_obj_add_event_cb(s_brightness_slider,
                        ControlCenter_BrightnessEvent,
                        LV_EVENT_RELEASED,
                        NULL);

    /* 整页从屏幕底部滑入，形成控制中心上拉效果。 */
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_root);
    lv_anim_set_exec_cb(&animation, ControlCenter_SetY);
    lv_anim_set_values(&animation, LCD_H, 0);
    lv_anim_set_time(&animation, 180U);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

void ControlCenterPage_Destroy(void)
{
    uint8_t ambient = DeviceManager_GetAmbientBrightness();

    /* 即使滑块没有产生 RELEASED，离开页面时也提交最终预览值。 */
    if(ambient > s_preview_brightness) ambient = s_preview_brightness;
    DeviceManager_SetBrightness(s_preview_brightness, ambient);
    LCD_Set_Light(DeviceManager_GetWorkingBrightness());

    lv_obj_clean(lv_scr_act());
    s_root = NULL;
    s_brightness_slider = NULL;
    s_brightness_value = NULL;
    s_flashlight_layer = NULL;
    s_power_msgbox = NULL;
    s_flashlight_active = 0U;
}

uint8_t ControlCenterPage_IsFlashlightActive(void)
{
    return s_flashlight_active;
}
