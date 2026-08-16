#include "watch_face.h"

#include "app_ui.h"
#include "lvgl.h"
#include "notification_manager.h"
#include "rtc.h"

#include <stdint.h>

#define WATCH_FACE_SECOND_PERIOD_MS   15U
#define WATCH_FACE_MINUTE_PERIOD_MS   45000U
#define WATCH_FACE_HOUR_PERIOD_MS     120000U
#define WATCH_FACE_DATE_Y_OFFSET      43
#define WATCH_FACE_SCALE_MAX          4320000
#define WATCH_FACE_UNITS_PER_SECOND   100U
#define WATCH_FACE_METER_PAD          9
#define WATCH_FACE_SECOND_RADIUS_MOD  (-17)
#define WATCH_FACE_SECOND_ANGLE_SCALE 100U
#define WATCH_FACE_SECOND_SEGMENTS    8U
#define WATCH_FACE_SECOND_WIDTH       2
#define WATCH_FACE_SECOND_DIRTY_PAD   3

static lv_obj_t *s_meter;
static lv_obj_t *s_date_label;
static lv_obj_t *s_center_dot;
static lv_obj_t *s_notification_dot;
static lv_meter_indicator_t *s_hour_hand;
static lv_meter_indicator_t *s_hour_hand_inner;
static lv_meter_indicator_t *s_minute_hand;
static lv_meter_indicator_t *s_minute_hand_inner;
static lv_obj_t *s_second_draw_obj;
static lv_timer_t *s_second_timer;
static lv_timer_t *s_minute_timer;
static lv_timer_t *s_hour_timer;
static lv_coord_t s_dial_center_x;
static lv_coord_t s_dial_center_y;
static lv_coord_t s_second_radius;
static uint16_t s_second_angle_x100;
static uint8_t s_second_angle_valid;
static uint8_t s_second_visible;
static uint8_t s_ambient;

static uint8_t s_last_year = 0xFFU;
static uint8_t s_last_month = 0xFFU;
static uint8_t s_last_date = 0xFFU;

static void WatchFace_UpdateSecond(lv_timer_t *timer);
static void WatchFace_UpdateMinute(lv_timer_t *timer);
static void WatchFace_UpdateHour(lv_timer_t *timer);
static uint8_t WatchFace_ReadRtc(RTC_TimeTypeDef *time,
                                 RTC_DateTypeDef *date);
static uint32_t WatchFace_GetFraction100(const RTC_TimeTypeDef *time);
static void WatchFace_SetSecondAngle(uint16_t angle_x100);
static void WatchFace_GetSecondTip(uint16_t angle_deg, lv_point_t *tip);
static void WatchFace_GetInterpolatedSecondTip(uint16_t angle_x100,
                                               lv_point_t *tip);
static void WatchFace_InvalidateSecond(uint16_t angle_x100);
static void WatchFace_SecondDrawEvent(lv_event_t *event);
static void WatchFace_UpdateDate(const RTC_DateTypeDef *date);
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
    if((s_second_timer != NULL) ||
       (s_minute_timer != NULL) ||
       (s_hour_timer != NULL)) {
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
    s_dial_center_x = width / 2;
    s_dial_center_y = height / 2;
    s_second_radius = (diameter / 2) +
                      WATCH_FACE_SECOND_RADIUS_MOD -
                      WATCH_FACE_METER_PAD;

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
    lv_obj_set_style_pad_all(s_meter, WATCH_FACE_METER_PAD, LV_PART_MAIN);

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
    /*
     * A transparent full-screen object draws the second hand, but only narrow
     * segment rectangles along the old/new hand are invalidated. The endpoint
     * is interpolated between adjacent integer angles for sub-degree motion.
     */
    s_second_draw_obj = lv_obj_create(screen);
    lv_obj_remove_style_all(s_second_draw_obj);
    lv_obj_set_pos(s_second_draw_obj, 0, 0);
    lv_obj_set_size(s_second_draw_obj, width, height);
    lv_obj_clear_flag(s_second_draw_obj,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_second_draw_obj,
                        WatchFace_SecondDrawEvent,
                        LV_EVENT_DRAW_MAIN,
                        NULL);
    s_second_angle_valid = 0U;
    s_second_visible = 1U;

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

    WatchFace_UpdateHour(NULL);
    WatchFace_UpdateMinute(NULL);
    WatchFace_UpdateSecond(NULL);

    s_second_timer = lv_timer_create(WatchFace_UpdateSecond,
                                     WATCH_FACE_SECOND_PERIOD_MS,
                                     NULL);
    s_minute_timer = lv_timer_create(WatchFace_UpdateMinute,
                                     WATCH_FACE_MINUTE_PERIOD_MS,
                                     NULL);
    s_hour_timer = lv_timer_create(WatchFace_UpdateHour,
                                   WATCH_FACE_HOUR_PERIOD_MS,
                                   NULL);
}

void WatchFace_Destroy(void)
{
    /* Stop all RTC/LVGL work before deleting its target objects. */
    if(s_second_timer != NULL) {
        lv_timer_del(s_second_timer);
        s_second_timer = NULL;
    }
    if(s_minute_timer != NULL) {
        lv_timer_del(s_minute_timer);
        s_minute_timer = NULL;
    }
    if(s_hour_timer != NULL) {
        lv_timer_del(s_hour_timer);
        s_hour_timer = NULL;
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
    s_second_draw_obj = NULL;
    s_dial_center_x = 0;
    s_dial_center_y = 0;
    s_second_radius = 0;
    s_second_angle_x100 = 0U;
    s_second_angle_valid = 0U;
    s_second_visible = 0U;
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

    if((s_meter == NULL) ||
       (s_second_draw_obj == NULL) ||
       (s_second_timer == NULL)) {
        return;
    }

    if(s_ambient != 0U) {
        if(s_second_angle_valid != 0U) {
            WatchFace_InvalidateSecond(s_second_angle_x100);
        }
        s_second_visible = 0U;
        lv_timer_pause(s_second_timer);
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
        s_second_visible = 1U;
        s_second_angle_valid = 0U;
        WatchFace_UpdateSecond(NULL);
        lv_timer_resume(s_second_timer);
        lv_timer_ready(s_second_timer);
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
        if((draw_part->sub_part_ptr == s_hour_hand) ||
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

static uint8_t WatchFace_ReadRtc(RTC_TimeTypeDef *time,
                                 RTC_DateTypeDef *date)
{
    if((time == NULL) || (date == NULL)) return 0U;
    if(HAL_RTC_GetTime(&hrtc, time, RTC_FORMAT_BIN) != HAL_OK) return 0U;
    if(HAL_RTC_GetDate(&hrtc, date, RTC_FORMAT_BIN) != HAL_OK) return 0U;
    return 1U;
}

static uint32_t WatchFace_GetFraction100(const RTC_TimeTypeDef *time)
{
    uint32_t fraction_100 = 0U;

    /* RTC subsecond counter runs down from SecondFraction to zero. */
    if((time != NULL) && (time->SubSeconds <= time->SecondFraction)) {
        fraction_100 = ((time->SecondFraction - time->SubSeconds) *
                        WATCH_FACE_UNITS_PER_SECOND) /
                       (time->SecondFraction + 1U);
    }
    return fraction_100;
}

static void WatchFace_GetSecondTip(uint16_t angle_deg, lv_point_t *tip)
{
    int32_t sin_value;
    int32_t cos_value;

    if(tip == NULL) return;

    sin_value = lv_trigo_sin((int16_t)angle_deg);
    cos_value = lv_trigo_sin((int16_t)((angle_deg + 90U) % 360U));
    tip->x = s_dial_center_x +
             (lv_coord_t)((s_second_radius * sin_value) / LV_TRIGO_SIN_MAX);
    tip->y = s_dial_center_y -
             (lv_coord_t)((s_second_radius * cos_value) / LV_TRIGO_SIN_MAX);
}

static void WatchFace_GetInterpolatedSecondTip(uint16_t angle_x100,
                                               lv_point_t *tip)
{
    lv_point_t low_tip;
    lv_point_t high_tip;
    uint16_t low_deg;
    uint16_t high_deg;
    uint16_t fraction;
    int32_t delta_x;
    int32_t delta_y;

    if(tip == NULL) return;

    low_deg = (uint16_t)(angle_x100 / WATCH_FACE_SECOND_ANGLE_SCALE);
    high_deg = (uint16_t)((low_deg + 1U) % 360U);
    fraction = (uint16_t)(angle_x100 % WATCH_FACE_SECOND_ANGLE_SCALE);
    WatchFace_GetSecondTip(low_deg, &low_tip);
    WatchFace_GetSecondTip(high_deg, &high_tip);

    delta_x = (int32_t)(high_tip.x - low_tip.x) * (int32_t)fraction;
    delta_y = (int32_t)(high_tip.y - low_tip.y) * (int32_t)fraction;
    delta_x += (delta_x >= 0) ?
               (int32_t)(WATCH_FACE_SECOND_ANGLE_SCALE / 2U) :
               -(int32_t)(WATCH_FACE_SECOND_ANGLE_SCALE / 2U);
    delta_y += (delta_y >= 0) ?
               (int32_t)(WATCH_FACE_SECOND_ANGLE_SCALE / 2U) :
               -(int32_t)(WATCH_FACE_SECOND_ANGLE_SCALE / 2U);

    tip->x = low_tip.x +
             (lv_coord_t)(delta_x / (int32_t)WATCH_FACE_SECOND_ANGLE_SCALE);
    tip->y = low_tip.y +
             (lv_coord_t)(delta_y / (int32_t)WATCH_FACE_SECOND_ANGLE_SCALE);
}

static void WatchFace_InvalidateSecond(uint16_t angle_x100)
{
    lv_point_t low_tip;
    lv_point_t high_tip;
    lv_point_t points[4];
    lv_area_t area;
    uint16_t low_deg;
    uint16_t high_deg;
    uint32_t segment;
    uint32_t point;

    if(s_second_draw_obj == NULL) return;

    low_deg = (uint16_t)(angle_x100 / WATCH_FACE_SECOND_ANGLE_SCALE);
    high_deg = (uint16_t)((low_deg + 1U) % 360U);
    WatchFace_GetSecondTip(low_deg, &low_tip);
    WatchFace_GetSecondTip(high_deg, &high_tip);

    for(segment = 0U; segment < WATCH_FACE_SECOND_SEGMENTS; ++segment) {
        points[0].x = s_dial_center_x +
                      (lv_coord_t)(((int32_t)(low_tip.x - s_dial_center_x) *
                                    (int32_t)segment) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[0].y = s_dial_center_y +
                      (lv_coord_t)(((int32_t)(low_tip.y - s_dial_center_y) *
                                    (int32_t)segment) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[1].x = s_dial_center_x +
                      (lv_coord_t)(((int32_t)(low_tip.x - s_dial_center_x) *
                                    (int32_t)(segment + 1U)) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[1].y = s_dial_center_y +
                      (lv_coord_t)(((int32_t)(low_tip.y - s_dial_center_y) *
                                    (int32_t)(segment + 1U)) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[2].x = s_dial_center_x +
                      (lv_coord_t)(((int32_t)(high_tip.x - s_dial_center_x) *
                                    (int32_t)segment) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[2].y = s_dial_center_y +
                      (lv_coord_t)(((int32_t)(high_tip.y - s_dial_center_y) *
                                    (int32_t)segment) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[3].x = s_dial_center_x +
                      (lv_coord_t)(((int32_t)(high_tip.x - s_dial_center_x) *
                                    (int32_t)(segment + 1U)) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);
        points[3].y = s_dial_center_y +
                      (lv_coord_t)(((int32_t)(high_tip.y - s_dial_center_y) *
                                    (int32_t)(segment + 1U)) /
                                   (int32_t)WATCH_FACE_SECOND_SEGMENTS);

        area.x1 = points[0].x;
        area.x2 = points[0].x;
        area.y1 = points[0].y;
        area.y2 = points[0].y;
        for(point = 1U; point < 4U; ++point) {
            area.x1 = LV_MIN(area.x1, points[point].x);
            area.x2 = LV_MAX(area.x2, points[point].x);
            area.y1 = LV_MIN(area.y1, points[point].y);
            area.y2 = LV_MAX(area.y2, points[point].y);
        }
        lv_area_increase(&area,
                         WATCH_FACE_SECOND_DIRTY_PAD,
                         WATCH_FACE_SECOND_DIRTY_PAD);
        lv_obj_invalidate_area(s_second_draw_obj, &area);
    }
}

static void WatchFace_SetSecondAngle(uint16_t angle_x100)
{
    angle_x100 %= (360U * WATCH_FACE_SECOND_ANGLE_SCALE);
    if(s_second_draw_obj == NULL) return;
    if((s_second_angle_valid != 0U) &&
       (s_second_angle_x100 == angle_x100)) return;

    if((s_second_angle_valid != 0U) && (s_second_visible != 0U)) {
        WatchFace_InvalidateSecond(s_second_angle_x100);
    }
    s_second_angle_x100 = angle_x100;
    s_second_angle_valid = 1U;
    if(s_second_visible != 0U) {
        WatchFace_InvalidateSecond(s_second_angle_x100);
    }
}

static void WatchFace_SecondDrawEvent(lv_event_t *event)
{
    lv_draw_ctx_t *draw_ctx;
    lv_draw_line_dsc_t line_dsc;
    lv_point_t center;
    lv_point_t tip;

    if((s_second_visible == 0U) || (s_second_angle_valid == 0U)) return;

    draw_ctx = lv_event_get_draw_ctx(event);
    if(draw_ctx == NULL) return;

    center.x = s_dial_center_x;
    center.y = s_dial_center_y;
    WatchFace_GetInterpolatedSecondTip(s_second_angle_x100, &tip);

    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0xFF4D5AU);
    line_dsc.width = WATCH_FACE_SECOND_WIDTH;
    line_dsc.round_start = 1U;
    line_dsc.round_end = 1U;
    line_dsc.opa = LV_OPA_COVER;
    lv_draw_line(draw_ctx, &line_dsc, &center, &tip);
}

static void WatchFace_UpdateDate(const RTC_DateTypeDef *date)
{
    if((date == NULL) || (s_date_label == NULL)) return;

    if((date->Year != s_last_year) ||
       (date->Month != s_last_month) ||
       (date->Date != s_last_date)) {
        s_last_year = date->Year;
        s_last_month = date->Month;
        s_last_date = date->Date;

        lv_label_set_text_fmt(s_date_label,
                              "20%02u-%02u-%02u",
                              (unsigned int)date->Year,
                              (unsigned int)date->Month,
                              (unsigned int)date->Date);
        lv_obj_align(s_date_label,
                     LV_ALIGN_CENTER,
                     0,
                     WATCH_FACE_DATE_Y_OFFSET);
    }
}

static void WatchFace_UpdateSecond(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t fraction_100;
    uint32_t second_100;
    uint16_t angle_x100;

    (void)timer;
    if((s_second_draw_obj == NULL) ||
       (WatchFace_ReadRtc(&time, &date) == 0U)) return;

    fraction_100 = WatchFace_GetFraction100(&time);
    second_100 = ((uint32_t)time.Seconds * WATCH_FACE_UNITS_PER_SECOND) +
                 fraction_100;
    angle_x100 = (uint16_t)((second_100 *
                             (360U * WATCH_FACE_SECOND_ANGLE_SCALE)) /
                            (60U * WATCH_FACE_UNITS_PER_SECOND));
    WatchFace_SetSecondAngle(angle_x100);
}

static void WatchFace_UpdateMinute(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t fraction_100;
    int32_t minute_value;

    (void)timer;
    if((s_meter == NULL) ||
       (WatchFace_ReadRtc(&time, &date) == 0U)) return;

    fraction_100 = WatchFace_GetFraction100(&time);
    minute_value = (((int32_t)time.Minutes * 60 *
                     (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                    ((int32_t)time.Seconds *
                     (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                    (int32_t)fraction_100) * 12;
    lv_meter_set_indicator_value(s_meter, s_minute_hand, minute_value);
    lv_meter_set_indicator_value(s_meter, s_minute_hand_inner, minute_value);
    WatchFace_UpdateDate(&date);
}

static void WatchFace_UpdateHour(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t fraction_100;
    int32_t hour_value;

    (void)timer;
    if((s_meter == NULL) ||
       (WatchFace_ReadRtc(&time, &date) == 0U)) return;

    fraction_100 = WatchFace_GetFraction100(&time);
    hour_value = ((int32_t)(time.Hours % 12U) * 3600 *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 ((int32_t)time.Minutes * 60 *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 ((int32_t)time.Seconds *
                  (int32_t)WATCH_FACE_UNITS_PER_SECOND) +
                 (int32_t)fraction_100;
    lv_meter_set_indicator_value(s_meter, s_hour_hand, hour_value);
    lv_meter_set_indicator_value(s_meter, s_hour_hand_inner, hour_value);
}
