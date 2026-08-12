#include "time_setting_page.h"

#include "app_ui.h"
#include "device_manager.h"
#include "lvgl.h"
#include "rtc.h"

#include <stdint.h>

static const char s_hour_options[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n"
    "12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
static const char s_minute_second_options[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n"
    "15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n"
    "30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n"
    "45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

static lv_obj_t *s_hour_roller;
static lv_obj_t *s_minute_roller;
static lv_obj_t *s_second_roller;
static lv_obj_t *s_status_label;

static lv_obj_t *TimeSettingPage_CreateRoller(lv_obj_t *screen,
                                              const char *options,
                                              lv_coord_t x);
static void TimeSettingPage_Confirm(lv_event_t *event);

void TimeSettingPage_Create(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *confirm_button;
    lv_obj_t *confirm_label;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "SET TIME");
    lv_obj_set_style_text_color(title, lv_color_hex(0xDCE2EAU), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 9);

    s_hour_roller = TimeSettingPage_CreateRoller(screen, s_hour_options, 5);
    s_minute_roller = TimeSettingPage_CreateRoller(screen,
                                                   s_minute_second_options,
                                                   86);
    s_second_roller = TimeSettingPage_CreateRoller(screen,
                                                   s_minute_second_options,
                                                   167);

    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
        time.Hours = 0U;
        time.Minutes = 0U;
        time.Seconds = 0U;
    }
    /* Always read date after time to unlock the STM32 RTC shadow registers. */
    (void)HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    lv_roller_set_selected(s_hour_roller, time.Hours, LV_ANIM_OFF);
    lv_roller_set_selected(s_minute_roller, time.Minutes, LV_ANIM_OFF);
    lv_roller_set_selected(s_second_roller, time.Seconds, LV_ANIM_OFF);

    confirm_button = lv_btn_create(screen);
    lv_obj_set_size(confirm_button, 118, 38);
    lv_obj_align(confirm_button, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(confirm_button, 19, LV_PART_MAIN);
    lv_obj_set_style_bg_color(confirm_button,
                              lv_color_hex(0x2878FFU),
                              LV_PART_MAIN);
    lv_obj_add_event_cb(confirm_button,
                        TimeSettingPage_Confirm,
                        LV_EVENT_CLICKED,
                        NULL);

    confirm_label = lv_label_create(confirm_button);
    lv_label_set_text(confirm_label, "CONFIRM");
    lv_obj_center(confirm_label);

    s_status_label = lv_label_create(screen);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label,
                                lv_color_hex(0xFF7070U),
                                LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -49);
}

void TimeSettingPage_Destroy(void)
{
    lv_obj_clean(lv_scr_act());
    s_hour_roller = NULL;
    s_minute_roller = NULL;
    s_second_roller = NULL;
    s_status_label = NULL;
}

static lv_obj_t *TimeSettingPage_CreateRoller(lv_obj_t *screen,
                                              const char *options,
                                              lv_coord_t x)
{
    lv_obj_t *roller = lv_roller_create(screen);

    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3U);
    lv_obj_set_size(roller, 68, 112);
    lv_obj_align(roller, LV_ALIGN_TOP_LEFT, x, 38);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x10151CU), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller,
                              lv_color_hex(0x2878FFU),
                              LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller,
                                lv_color_hex(0x87909BU),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(roller,
                                lv_color_hex(0xFFFFFFU),
                                LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 8, LV_PART_MAIN);
    return roller;
}

static void TimeSettingPage_Confirm(lv_event_t *event)
{
    RTC_TimeTypeDef time = {0};

    (void)event;
    time.Hours = (uint8_t)lv_roller_get_selected(s_hour_roller);
    time.Minutes = (uint8_t)lv_roller_get_selected(s_minute_roller);
    time.Seconds = (uint8_t)lv_roller_get_selected(s_second_roller);
    time.TimeFormat = RTC_HOURFORMAT12_AM;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;

    if(HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) {
        DeviceManager_SaveDateTimeNow();
        AppUI_RequestPage(APP_UI_PAGE_WATCH);
    }
    else {
        lv_label_set_text(s_status_label, "RTC ERROR");
    }
}
