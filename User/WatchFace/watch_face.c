#include "watch_face.h"

#include "app_ui.h"
#include "lvgl.h"
#include "notification_manager.h"
#include "rtc.h"

#include <stdint.h>

#define WATCH_FACE_UPDATE_PERIOD_MS   25U
#define WATCH_FACE_AMBIENT_PERIOD_MS  60000U
#define WATCH_FACE_DATE_Y_OFFSET      43
#define WATCH_FACE_SCALE_MAX          4320000
#define WATCH_FACE_UNITS_PER_SECOND   100U

static lv_obj_t *s_meter;
static lv_obj_t *s_date_label;
static lv_obj_t *s_center_dot;
static lv_obj_t *s_notification_dot;
static lv_meter_indicator_t *s_hour_hand;
static lv_meter_indicator_t *s_hour_hand_inner;
static lv_meter_indicator_t *s_minute_hand;
static lv_meter_indicator_t *s_minute_hand_inner;
static lv_meter_indicator_t *s_second_hand;
static lv_timer_t *s_update_timer;
static uint8_t s_ambient;

static uint8_t s_last_year = 0xFFU;
static uint8_t s_last_month = 0xFFU;
static uint8_t s_last_date = 0xFFU;

static void WatchFace_Update(lv_timer_t *timer);
static void WatchFace_MeterDrawEvent(lv_event_t *event);
static void WatchFace_ScreenEvent(lv_event_t *event);

void WatchFace_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_meter_scale_t *scale;
    lv_coord_t width;
    lv_coord_t height;
    lv_coord_t diameter;

    /* Make repeated creation safe even if the caller forgot to destroy first. */
    if(s_update_timer != NULL) {
        WatchFace_Destroy();
    }

    /* This module owns the initial screen. */
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

    /* The active screen object is reused by every page. Remove stale copies
       before adding ours, otherwise each watch recreation stacks a callback. */
    while(lv_obj_remove_event_cb(screen, WatchFace_ScreenEvent)) {
    }
    lv_obj_add_event_cb(screen,
                        WatchFace_ScreenEvent,
                        LV_EVENT_LONG_PRESSED,
                        NULL);
    lv_obj_add_event_cb(screen,
                        WatchFace_ScreenEvent,
                        LV_EVENT_GESTURE,
                        NULL);
    s_ambient = 0U;

    lv_obj_update_layout(screen);
    width = lv_obj_get_width(screen);
    height = lv_obj_get_height(screen);
    diameter = ((width < height) ? width : height) - 10;

    /* Create the date before the meter so the hands are always drawn over it. */
    s_date_label = lv_label_create(screen);
    lv_label_set_text(s_date_label, "2000-01-01");
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xB6BDC6U), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_date_label, LV_OPA_50, LV_PART_MAIN);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, WATCH_FACE_DATE_Y_OFFSET);

    s_meter = lv_meter_create(screen);
    lv_obj_add_event_cb(s_meter,
                        WatchFace_MeterDrawEvent,
                        LV_EVENT_DRAW_PART_BEGIN,
                        NULL);
    lv_obj_set_size(s_meter, diameter, diameter);
    lv_obj_center(s_meter);
    lv_obj_clear_flag(s_meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_meter, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_meter, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_meter, 9, LV_PART_MAIN);

    scale = lv_meter_add_scale(s_meter);
    /*
     * One unit is 10 ms and the full range is 12 hours. 4,320,000 also
     * keeps LVGL 8.2's 32-bit lv_map(value * 360) arithmetic overflow-safe.
     */
    lv_meter_set_scale_range(s_meter,
                             scale,
                             0,
                             WATCH_FACE_SCALE_MAX,
                             360,
                             270);
    lv_meter_set_scale_ticks(s_meter,
                             scale,
                             61,
                             1,
                             7,
                             lv_color_hex(0x59616BU));
    lv_meter_set_scale_major_ticks(s_meter,
                                   scale,
                                   5,
                                   3,
                                   13,
                                   lv_color_hex(0xE7EBF0U),
                                   8);

    /* Drawing order: hour below minute, red second hand always on top. */
    s_hour_hand = lv_meter_add_needle_line(s_meter,
                                           scale,
                                           6,
                                           lv_color_hex(0xFFFFFFU),
                                           -54);
    s_hour_hand_inner = lv_meter_add_needle_line(s_meter,
                                                 scale,
                                                 3,
                                                 lv_color_hex(0xFFFFFFU),
                                                 -54);
    s_minute_hand = lv_meter_add_needle_line(s_meter,
                                             scale,
                                             4,
                                             lv_color_hex(0xDDE3EAU),
                                             -27);
    s_minute_hand_inner = lv_meter_add_needle_line(s_meter,
                                                   scale,
                                                   2,
                                                   lv_color_hex(0xDDE3EAU),
                                                   -27);
    s_second_hand = lv_meter_add_needle_line(s_meter,
                                             scale,
                                             2,
                                             lv_color_hex(0xFF4D5AU),
                                             -17);

    s_center_dot = lv_obj_create(screen);
    lv_obj_set_size(s_center_dot, 12, 12);
    lv_obj_set_style_radius(s_center_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_center_dot, lv_color_hex(0xFF4D5AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_center_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_center_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_center_dot, 0, LV_PART_MAIN);
    lv_obj_center(s_center_dot);
    lv_obj_clear_flag(s_center_dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_notification_dot = lv_obj_create(screen);
    lv_obj_set_size(s_notification_dot, 9, 9);
    lv_obj_align(s_notification_dot, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_clear_flag(s_notification_dot,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_notification_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_notification_dot,
                              lv_color_hex(0xFF3038U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_notification_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_notification_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_notification_dot, 0, LV_PART_MAIN);
    WatchFace_RefreshNotificationIndicator();

    WatchFace_Update(NULL);

    if(s_update_timer == NULL) {
        s_update_timer = lv_timer_create(WatchFace_Update,
                                         WATCH_FACE_UPDATE_PERIOD_MS,
                                         NULL);
    }
}

void WatchFace_Destroy(void)
{
    /* Stop the 20 FPS RTC/LVGL work before deleting its target objects. */
    if(s_update_timer != NULL) {
        lv_timer_del(s_update_timer);
        s_update_timer = NULL;
    }

    /* lv_obj_clean() removes children only; root-screen callbacks survive it. */
    while(lv_obj_remove_event_cb(lv_scr_act(), WatchFace_ScreenEvent)) {
    }
    lv_obj_clean(lv_scr_act());

    s_meter = NULL;
    s_date_label = NULL;
    s_center_dot = NULL;
    s_notification_dot = NULL;
    s_hour_hand = NULL;
    s_hour_hand_inner = NULL;
    s_minute_hand = NULL;
    s_minute_hand_inner = NULL;
    s_second_hand = NULL;
    s_ambient = 0U;

    /* Force the date label to be initialized again on the next entry. */
    s_last_year = 0xFFU;
    s_last_month = 0xFFU;
    s_last_date = 0xFFU;
}

void WatchFace_RefreshNotificationIndicator(void)
{
    if(s_notification_dot == NULL) return;
    if(NotificationManager_GetCount() != 0U) {
        lv_obj_clear_flag(s_notification_dot, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(s_notification_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

void WatchFace_SetAmbientMode(uint8_t enabled)
{
    s_ambient = (enabled != 0U) ? 1U : 0U;

    if((s_meter == NULL) || (s_update_timer == NULL)) {
        return;
    }

    lv_timer_set_period(s_update_timer,
                        (s_ambient != 0U) ?
                        WATCH_FACE_AMBIENT_PERIOD_MS :
                        WATCH_FACE_UPDATE_PERIOD_MS);
    lv_timer_ready(s_update_timer);

    if(s_ambient != 0U) {
        lv_obj_set_style_bg_color(s_center_dot,
                                  lv_color_hex(0x000000),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_center_dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_center_dot, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_center_dot,
                                      lv_color_hex(0xAAB1BAU),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(s_center_dot, 90, LV_PART_MAIN);
    }
    else {
        lv_obj_set_style_bg_color(s_center_dot,
                                  lv_color_hex(0xFF4D5AU),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_center_dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_center_dot, 0, LV_PART_MAIN);
    }

    lv_obj_invalidate(s_meter);
    lv_obj_invalidate(s_center_dot);
}

static void WatchFace_MeterDrawEvent(lv_event_t *event)
{
    lv_obj_draw_part_dsc_t *draw_part;

    draw_part = lv_event_get_draw_part_dsc(event);

    /* Keep major tick marks, but hide the meter's internal numeric labels. */
    if((draw_part != NULL) &&
       (draw_part->type == LV_METER_DRAW_PART_TICK) &&
       (draw_part->text != NULL)) {
        draw_part->text = "";
    }

    if(draw_part == NULL) {
        return;
    }

    if((s_ambient != 0U) &&
       (draw_part->type == LV_METER_DRAW_PART_TICK) &&
       (draw_part->line_dsc != NULL)) {
        draw_part->line_dsc->opa = LV_OPA_TRANSP;
    }

    if((draw_part->id == LV_METER_DRAW_PART_NEEDLE_LINE) &&
       (draw_part->line_dsc != NULL)) {
        if(draw_part->sub_part_ptr == s_second_hand) {
            draw_part->line_dsc->opa =
                (s_ambient != 0U) ? LV_OPA_TRANSP : LV_OPA_COVER;
        }
        else if((draw_part->sub_part_ptr == s_hour_hand) ||
                (draw_part->sub_part_ptr == s_minute_hand)) {
            draw_part->line_dsc->opa =
                (s_ambient != 0U) ? 90 : LV_OPA_COVER;
        }
        else if((draw_part->sub_part_ptr == s_hour_hand_inner) ||
                (draw_part->sub_part_ptr == s_minute_hand_inner)) {
            if(s_ambient != 0U) {
                draw_part->line_dsc->color = lv_color_hex(0x05070AU);
            }
            draw_part->line_dsc->opa = LV_OPA_COVER;
        }
    }
}

static void WatchFace_ScreenEvent(lv_event_t *event)
{
    lv_indev_t *indev;
    lv_event_code_t code = lv_event_get_code(event);

    if((code == LV_EVENT_GESTURE) &&
       (AppUI_GetCurrentPage() == APP_UI_PAGE_WATCH)) {
        indev = lv_indev_get_act();
        if((indev != NULL) &&
           (lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM) &&
           (NotificationManager_GetCount() != 0U)) {
            lv_indev_wait_release(indev);
            AppUI_RequestPage(APP_UI_PAGE_NOTIFICATIONS);
        }
        return;
    }

    if((code == LV_EVENT_LONG_PRESSED) &&
       (AppUI_GetCurrentPage() == APP_UI_PAGE_WATCH) &&
       (AppUI_IsAmbient() == 0U)) {
        /* Do not let the same physical hold continue on the newly built page. */
        indev = lv_indev_get_act();
        if(indev != NULL) {
            lv_indev_wait_release(indev);
        }
        AppUI_RequestPage(APP_UI_PAGE_TIME_SETTING);
    }
}

static void WatchFace_Update(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t fraction_10ms = 0U;
    int32_t second_value;
    int32_t minute_value;
    int32_t hour_value;

    (void)timer;

    if((s_meter == NULL) ||
       (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
       (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)) {
        return;
    }

    /* RTC subsecond counter runs down from SecondFraction to zero. */
    if(time.SubSeconds <= time.SecondFraction) {
        fraction_10ms = ((time.SecondFraction - time.SubSeconds) *
                         WATCH_FACE_UNITS_PER_SECOND) /
                        (time.SecondFraction + 1U);
    }

    /*
     * All hands share the same 12-hour scale:
     * second hand: one revolution per minute;
     * minute hand: one revolution per hour;
     * hour hand:   one revolution per 12 hours.
     */
    second_value = (((int32_t)time.Seconds *
                     (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                    (int32_t)fraction_10ms) * 720;

    minute_value = (((int32_t)time.Minutes * 60 *
                     (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                    ((int32_t)time.Seconds *
                     (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                    (int32_t)fraction_10ms) * 12;

    hour_value = ((int32_t)(time.Hours % 12U) * 3600 *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 ((int32_t)time.Minutes * 60 *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 ((int32_t)time.Seconds *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 (int32_t)fraction_10ms;

    if(s_ambient == 0U) {
        lv_meter_set_indicator_value(s_meter, s_second_hand, second_value);
    }
    lv_meter_set_indicator_value(s_meter, s_minute_hand, minute_value);
    lv_meter_set_indicator_value(s_meter, s_minute_hand_inner, minute_value);
    lv_meter_set_indicator_value(s_meter, s_hour_hand, hour_value);
    lv_meter_set_indicator_value(s_meter, s_hour_hand_inner, hour_value);

    /* Date changes once a day, so avoid reallocating its text every second. */
    if((date.Year != s_last_year) ||
       (date.Month != s_last_month) ||
       (date.Date != s_last_date)) {
        s_last_year = date.Year;
        s_last_month = date.Month;
        s_last_date = date.Date;

        lv_label_set_text_fmt(s_date_label,
                              "20%02u-%02u-%02u",
                              (unsigned int)date.Year,
                              (unsigned int)date.Month,
                              (unsigned int)date.Date);
        lv_obj_align(s_date_label,
                     LV_ALIGN_CENTER,
                     0,
                     WATCH_FACE_DATE_Y_OFFSET);
    }
}
