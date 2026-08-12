#include "date_setting_page.h"

#include "app_ui.h"
#include "device_manager.h"
#include "lvgl.h"
#include "rtc.h"

#include <stdint.h>

static lv_obj_t *s_year_roller;
static lv_obj_t *s_month_roller;
static lv_obj_t *s_day_roller;
static lv_obj_t *s_status_label;
static char s_year_options[500];
static char s_month_options[36];
static char s_day_options[93];
static uint8_t s_updating_day;

static void DateSettingPage_MakeOptions(char *buffer,
                                        uint16_t first,
                                        uint16_t last,
                                        uint8_t four_digits);
static uint8_t DateSettingPage_DaysInMonth(uint16_t year, uint8_t month);
static uint8_t DateSettingPage_Weekday(uint16_t year,
                                       uint8_t month,
                                       uint8_t day);
static void DateSettingPage_UpdateDays(void);
static void DateSettingPage_DateChanged(lv_event_t *event);
static void DateSettingPage_Confirm(lv_event_t *event);
static lv_obj_t *DateSettingPage_CreateRoller(lv_obj_t *screen,
                                              const char *options,
                                              lv_coord_t x);

void DateSettingPage_Create(void)
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

    DateSettingPage_MakeOptions(s_year_options, 2000U, 2099U, 1U);
    DateSettingPage_MakeOptions(s_month_options, 1U, 12U, 0U);

    title = lv_label_create(screen);
    lv_label_set_text(title, "SET DATE");
    lv_obj_set_style_text_color(title, lv_color_hex(0xDCE2EAU), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 9);

    s_year_roller = DateSettingPage_CreateRoller(screen, s_year_options, 5);
    s_month_roller = DateSettingPage_CreateRoller(screen, s_month_options, 86);
    s_day_roller = DateSettingPage_CreateRoller(screen, "01", 167);

    (void)HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    if(HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        date.Year = 0U;
        date.Month = 1U;
        date.Date = 1U;
    }

    lv_roller_set_selected(s_year_roller, date.Year, LV_ANIM_OFF);
    lv_roller_set_selected(s_month_roller, date.Month - 1U, LV_ANIM_OFF);
    DateSettingPage_UpdateDays();
    lv_roller_set_selected(s_day_roller, date.Date - 1U, LV_ANIM_OFF);

    lv_obj_add_event_cb(s_year_roller,
                        DateSettingPage_DateChanged,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);
    lv_obj_add_event_cb(s_month_roller,
                        DateSettingPage_DateChanged,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);

    confirm_button = lv_btn_create(screen);
    lv_obj_set_size(confirm_button, 118, 38);
    lv_obj_align(confirm_button, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(confirm_button, 19, LV_PART_MAIN);
    lv_obj_set_style_bg_color(confirm_button,
                              lv_color_hex(0x2878FFU),
                              LV_PART_MAIN);
    lv_obj_add_event_cb(confirm_button,
                        DateSettingPage_Confirm,
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

void DateSettingPage_Destroy(void)
{
    lv_obj_clean(lv_scr_act());
    s_year_roller = NULL;
    s_month_roller = NULL;
    s_day_roller = NULL;
    s_status_label = NULL;
    s_updating_day = 0U;
}

static lv_obj_t *DateSettingPage_CreateRoller(lv_obj_t *screen,
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

static void DateSettingPage_MakeOptions(char *buffer,
                                        uint16_t first,
                                        uint16_t last,
                                        uint8_t four_digits)
{
    uint16_t value;
    uint16_t pos = 0U;

    for(value = first; value <= last; value++) {
        if(four_digits != 0U) {
            buffer[pos++] = (char)('0' + ((value / 1000U) % 10U));
            buffer[pos++] = (char)('0' + ((value / 100U) % 10U));
        }
        buffer[pos++] = (char)('0' + ((value / 10U) % 10U));
        buffer[pos++] = (char)('0' + (value % 10U));
        if(value != last) {
            buffer[pos++] = '\n';
        }
    }
    buffer[pos] = '\0';
}

static uint8_t DateSettingPage_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };
    uint8_t result = days[month - 1U];

    if((month == 2U) &&
       (((year % 400U) == 0U) ||
        (((year % 4U) == 0U) && ((year % 100U) != 0U)))) {
        result = 29U;
    }
    return result;
}

static void DateSettingPage_UpdateDays(void)
{
    uint16_t year = 2000U + lv_roller_get_selected(s_year_roller);
    uint8_t month = (uint8_t)(lv_roller_get_selected(s_month_roller) + 1U);
    uint8_t old_day = (uint8_t)(lv_roller_get_selected(s_day_roller) + 1U);
    uint8_t day_count = DateSettingPage_DaysInMonth(year, month);

    if(old_day > day_count) {
        old_day = day_count;
    }

    s_updating_day = 1U;
    DateSettingPage_MakeOptions(s_day_options, 1U, day_count, 0U);
    lv_roller_set_options(s_day_roller, s_day_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(s_day_roller, old_day - 1U, LV_ANIM_OFF);
    s_updating_day = 0U;
}

static void DateSettingPage_DateChanged(lv_event_t *event)
{
    (void)event;
    if(s_updating_day == 0U) {
        DateSettingPage_UpdateDays();
    }
}

static uint8_t DateSettingPage_Weekday(uint16_t year,
                                       uint8_t month,
                                       uint8_t day)
{
    static const uint8_t month_table[] = {
        0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U
    };
    uint32_t y = year;
    uint32_t weekday;

    if(month < 3U) {
        y--;
    }
    weekday = (y + y / 4U - y / 100U + y / 400U +
               month_table[month - 1U] + day) % 7U;
    return (weekday == 0U) ? RTC_WEEKDAY_SUNDAY : (uint8_t)weekday;
}

static void DateSettingPage_Confirm(lv_event_t *event)
{
    RTC_DateTypeDef date = {0};
    uint16_t year;

    (void)event;
    year = 2000U + lv_roller_get_selected(s_year_roller);
    date.Year = (uint8_t)(year - 2000U);
    date.Month = (uint8_t)(lv_roller_get_selected(s_month_roller) + 1U);
    date.Date = (uint8_t)(lv_roller_get_selected(s_day_roller) + 1U);
    date.WeekDay = DateSettingPage_Weekday(year, date.Month, date.Date);

    if(HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK) {
        DeviceManager_SaveDateTimeNow();
        AppUI_RequestPage(APP_UI_PAGE_MENU);
    }
    else {
        lv_label_set_text(s_status_label, "RTC ERROR");
    }
}
