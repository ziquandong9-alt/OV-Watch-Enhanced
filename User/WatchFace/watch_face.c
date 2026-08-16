#include "watch_face.h"

#include "app_ui.h"
#include "battery_manager.h"
#include "device_manager.h"
#include "lvgl.h"
#include "menu_icons.h"
#include "notification_manager.h"
#include "rtc.h"
#include "status_bar.h"

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
#define WATCH_FACE_INFO_PERIOD_MS      500U
#define WATCH_FACE_COUNT               3U
#define WATCH_FACE_INFO_INDEX          1U
#define WATCH_FACE_JELLY_INDEX         2U

#define INFO_COLOR_BLUE                0x3278FFU
#define INFO_COLOR_ORANGE              0xF5A73AU
#define INFO_COLOR_CYAN                0x14C8E1U
#define INFO_COLOR_RED                 0xE11432U

LV_FONT_DECLARE(ui_font_iconfont16);
LV_FONT_DECLARE(ui_font_iconfont24);
LV_FONT_DECLARE(ui_font_iconfont28);
LV_FONT_DECLARE(ui_font_Cuyuan135);

#define INFO_ICON_BATTERY              "\xEE\x9A\x8F" /* U+E68F */
#define INFO_ICON_STEPS                "\xEE\x99\xB6" /* U+E676 */
#define INFO_ICON_TEMPERATURE          "\xEE\x99\x99" /* U+E659 */
#define INFO_ICON_HUMIDITY             "\xEE\x99\xB3" /* U+E673 */

static lv_obj_t *s_meter;
static lv_obj_t *s_pager;
static lv_obj_t *s_analog_page;
static lv_obj_t *s_info_page;
static lv_obj_t *s_jelly_page;
static lv_obj_t *s_info_content;
static lv_obj_t *s_info_time;
static lv_obj_t *s_info_date;
static lv_obj_t *s_info_battery_arc;
static lv_obj_t *s_info_battery_value;
static lv_obj_t *s_info_steps_value;
static lv_obj_t *s_info_steps_bar;
static lv_obj_t *s_info_temp_arc;
static lv_obj_t *s_info_temp_value;
static lv_obj_t *s_info_humi_arc;
static lv_obj_t *s_info_humi_value;
static lv_obj_t *s_info_heart_arc;
static lv_obj_t *s_info_heart_value;
static lv_obj_t *s_info_ambient_group;
static lv_obj_t *s_info_ambient_labels[9];
static lv_obj_t *s_date_label;
static lv_obj_t *s_center_dot;
static lv_obj_t *s_notification_dot;
static lv_obj_t *s_info_notification_dot;
static lv_obj_t *s_jelly_notification_dot;
/* Four independent digits: hour tens/ones, minute tens/ones. Layer 8 is
   the black cutout used only in ambient mode. */
static lv_obj_t *s_jelly_digit_labels[4][9];
static lv_meter_indicator_t *s_hour_hand;
static lv_meter_indicator_t *s_hour_hand_inner;
static lv_meter_indicator_t *s_minute_hand;
static lv_meter_indicator_t *s_minute_hand_inner;
static lv_obj_t *s_second_draw_obj;
static lv_timer_t *s_second_timer;
static lv_timer_t *s_minute_timer;
static lv_timer_t *s_hour_timer;
static lv_timer_t *s_info_timer;
static lv_timer_t *s_jelly_timer;
static lv_coord_t s_dial_center_x;
static lv_coord_t s_dial_center_y;
static lv_coord_t s_second_radius;
static uint16_t s_second_angle_x100;
static uint8_t s_second_angle_valid;
static uint8_t s_second_visible;
static uint8_t s_ambient;
/* Deliberately survives WatchFace_Destroy(): page recreation and STOP wake
   return to the face that the user last selected. */
static uint8_t s_selected_face;

static uint8_t s_last_year = 0xFFU;
static uint8_t s_last_month = 0xFFU;
static uint8_t s_last_date = 0xFFU;
static uint8_t s_info_last_hour = 0xFFU;
static uint8_t s_info_last_minute = 0xFFU;
static uint8_t s_info_last_date = 0xFFU;
static uint8_t s_info_last_battery = 0xFFU;
static uint8_t s_info_last_battery_present = 0xFFU;
static uint32_t s_info_last_steps = 0xFFFFFFFFUL;
static int16_t s_info_last_temp = INT16_MIN;
static int16_t s_info_last_humi = INT16_MIN;
static uint8_t s_info_last_temp_connected = 0xFFU;
static uint8_t s_info_last_humi_connected = 0xFFU;
static uint16_t s_info_last_heart = 0xFFFFU;
static uint8_t s_info_last_heart_connected = 0xFFU;
static uint8_t s_jelly_last_hour = 0xFFU;
static uint8_t s_jelly_last_minute = 0xFFU;

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
static void WatchFace_PagerEvent(lv_event_t *event);
static void WatchFace_CreateInfo(lv_obj_t *parent);
static void WatchFace_UpdateInfo(lv_timer_t *timer);
static void WatchFace_InfoClickEvent(lv_event_t *event);
static void WatchFace_SetInfoAmbient(uint8_t enabled);
static void WatchFace_UpdateInfoTime(const RTC_TimeTypeDef *time,
                                     const RTC_DateTypeDef *date);
static lv_obj_t *WatchFace_CreateInfoArc(lv_obj_t *parent,
                                         lv_coord_t x,
                                         uint32_t color,
                                         int32_t min,
                                         int32_t max);
static void WatchFace_CreateInfoHitArea(lv_obj_t *parent,
                                        lv_coord_t x,
                                        lv_coord_t y,
                                        lv_coord_t width,
                                        lv_coord_t height,
                                        AppUI_Page_t page);
static void WatchFace_ApplySelectedTimers(void);
static void WatchFace_CreateJelly(lv_obj_t *parent);
static void WatchFace_UpdateJelly(lv_timer_t *timer);
static void WatchFace_SetJellyAmbient(uint8_t enabled);
static void WatchFace_ScheduleNextMinute(lv_timer_t *timer,
                                         const RTC_TimeTypeDef *time);
static void WatchFace_ApplyStatusBar(void);

void WatchFace_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_meter_scale_t *scale;
    lv_coord_t width;
    lv_coord_t height;
    lv_coord_t diameter;

    s_selected_face = DeviceManager_GetWatchFace();

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

    s_pager = lv_obj_create(screen);
    lv_obj_set_pos(s_pager, 0, 0);
    lv_obj_set_size(s_pager, width, height);
    lv_obj_set_flex_flow(s_pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(s_pager, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(s_pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(s_pager, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(s_pager, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(s_pager, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(s_pager, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_pager, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_pager, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_pager, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_pager,
                        WatchFace_PagerEvent,
                        LV_EVENT_SCROLL_END,
                        NULL);
    lv_obj_add_event_cb(s_pager,
                        WatchFace_ScreenEvent,
                        LV_EVENT_LONG_PRESSED,
                        NULL);
    lv_obj_add_event_cb(s_pager,
                        WatchFace_ScreenEvent,
                        LV_EVENT_GESTURE,
                        NULL);

    s_analog_page = lv_obj_create(s_pager);
    lv_obj_set_size(s_analog_page, width, height);
    lv_obj_clear_flag(s_analog_page,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_analog_page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_analog_page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_analog_page, 0, LV_PART_MAIN);

    /* Create the date before the meter so the hands are always drawn over it. */
    s_date_label = lv_label_create(s_analog_page);
    lv_label_set_text(s_date_label, "2000-01-01");
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xB6BDC6U), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_date_label, LV_OPA_50, LV_PART_MAIN);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, WATCH_FACE_DATE_Y_OFFSET);

    s_meter = lv_meter_create(s_analog_page);
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
    s_second_draw_obj = lv_obj_create(s_analog_page);
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

    s_center_dot = lv_obj_create(s_analog_page);
    lv_obj_set_size(s_center_dot, 12, 12);
    lv_obj_set_style_radius(s_center_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_center_dot, lv_color_hex(0xFF4D5AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_center_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_center_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_center_dot, 0, LV_PART_MAIN);
    lv_obj_center(s_center_dot);
    lv_obj_clear_flag(s_center_dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_notification_dot = lv_obj_create(s_analog_page);
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

    s_info_page = lv_obj_create(s_pager);
    lv_obj_set_size(s_info_page, width, height);
    lv_obj_clear_flag(s_info_page,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_info_page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_page, 0, LV_PART_MAIN);
    WatchFace_CreateInfo(s_info_page);

    s_info_notification_dot = lv_obj_create(s_info_page);
    lv_obj_set_size(s_info_notification_dot, 9, 9);
    lv_obj_align(s_info_notification_dot, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_clear_flag(s_info_notification_dot,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_info_notification_dot,
                            LV_RADIUS_CIRCLE,
                            LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_info_notification_dot,
                              lv_color_hex(0xFF3038U),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_info_notification_dot,
                            LV_OPA_COVER,
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_notification_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_notification_dot, 0, LV_PART_MAIN);
    WatchFace_RefreshNotificationIndicator();

    s_jelly_page = lv_obj_create(s_pager);
    lv_obj_set_size(s_jelly_page, width, height);
    lv_obj_clear_flag(s_jelly_page,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_jelly_page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_jelly_page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_jelly_page, 0, LV_PART_MAIN);
    WatchFace_CreateJelly(s_jelly_page);
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
    s_info_timer = lv_timer_create(WatchFace_UpdateInfo,
                                   WATCH_FACE_INFO_PERIOD_MS,
                                   NULL);
    s_jelly_timer = lv_timer_create(WatchFace_UpdateJelly,
                                    1000U,
                                    NULL);

    lv_obj_update_layout(s_pager);
    if(s_selected_face == WATCH_FACE_JELLY_INDEX) {
        lv_obj_scroll_to_view(s_jelly_page, LV_ANIM_OFF);
    }
    else if(s_selected_face == WATCH_FACE_INFO_INDEX) {
        lv_obj_scroll_to_view(s_info_page, LV_ANIM_OFF);
    }
    else {
        lv_obj_scroll_to_view(s_analog_page, LV_ANIM_OFF);
    }
    WatchFace_UpdateInfo(NULL);
    WatchFace_UpdateJelly(NULL);
    WatchFace_ApplySelectedTimers();
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
    if(s_info_timer != NULL) {
        lv_timer_del(s_info_timer);
        s_info_timer = NULL;
    }
    if(s_jelly_timer != NULL) {
        lv_timer_del(s_jelly_timer);
        s_jelly_timer = NULL;
    }

    /* lv_obj_clean() removes children only; root-screen callbacks survive it. */
    while(lv_obj_remove_event_cb(lv_scr_act(), WatchFace_ScreenEvent)) {
    }
    lv_obj_clean(lv_scr_act());

    s_meter = NULL;
    s_pager = NULL;
    s_analog_page = NULL;
    s_info_page = NULL;
    s_jelly_page = NULL;
    s_info_content = NULL;
    s_info_time = NULL;
    s_info_date = NULL;
    s_info_battery_arc = NULL;
    s_info_battery_value = NULL;
    s_info_steps_value = NULL;
    s_info_steps_bar = NULL;
    s_info_temp_arc = NULL;
    s_info_temp_value = NULL;
    s_info_humi_arc = NULL;
    s_info_humi_value = NULL;
    s_info_heart_arc = NULL;
    s_info_heart_value = NULL;
    s_info_ambient_group = NULL;
    {
        uint32_t i;
        for(i = 0U; i < 9U; ++i) s_info_ambient_labels[i] = NULL;
    }
    s_date_label = NULL;
    s_center_dot = NULL;
    s_notification_dot = NULL;
    s_info_notification_dot = NULL;
    s_jelly_notification_dot = NULL;
    {
        uint32_t digit;
        uint32_t layer;
        for(digit = 0U; digit < 4U; ++digit) {
            for(layer = 0U; layer < 9U; ++layer) {
                s_jelly_digit_labels[digit][layer] = NULL;
            }
        }
    }
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
    s_info_last_hour = 0xFFU;
    s_info_last_minute = 0xFFU;
    s_info_last_date = 0xFFU;
    s_info_last_battery = 0xFFU;
    s_info_last_battery_present = 0xFFU;
    s_info_last_steps = 0xFFFFFFFFUL;
    s_info_last_temp = INT16_MIN;
    s_info_last_humi = INT16_MIN;
    s_info_last_temp_connected = 0xFFU;
    s_info_last_humi_connected = 0xFFU;
    s_info_last_heart = 0xFFFFU;
    s_info_last_heart_connected = 0xFFU;
    s_jelly_last_hour = 0xFFU;
    s_jelly_last_minute = 0xFFU;
}

void WatchFace_RefreshNotificationIndicator(void)
{
    if(NotificationManager_GetCount() != 0U) {
        if(s_notification_dot != NULL) {
            lv_obj_clear_flag(s_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
        if((s_info_notification_dot != NULL) && (s_ambient == 0U)) {
            lv_obj_clear_flag(s_info_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
        if((s_jelly_notification_dot != NULL) && (s_ambient == 0U)) {
            lv_obj_clear_flag(s_jelly_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else {
        if(s_notification_dot != NULL) {
            lv_obj_add_flag(s_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
        if(s_info_notification_dot != NULL) {
            lv_obj_add_flag(s_info_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
        if(s_jelly_notification_dot != NULL) {
            lv_obj_add_flag(s_jelly_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

uint8_t WatchFace_GetSelectedIndex(void)
{
    return s_selected_face;
}

void WatchFace_SetAmbientMode(uint8_t enabled)
{
    s_ambient = (enabled != 0U) ? 1U : 0U;

    if((s_meter == NULL) ||
       (s_second_draw_obj == NULL) ||
       (s_second_timer == NULL) ||
       (s_pager == NULL)) {
        return;
    }

    if(s_ambient != 0U) {
        if(s_second_angle_valid != 0U) {
            WatchFace_InvalidateSecond(s_second_angle_x100);
        }
        s_second_visible = 0U;
        lv_timer_pause(s_second_timer);
        lv_obj_clear_flag(s_pager, LV_OBJ_FLAG_SCROLLABLE);
        WatchFace_SetInfoAmbient(1U);
        WatchFace_SetJellyAmbient(1U);
        if(s_info_notification_dot != NULL) {
            lv_obj_add_flag(s_info_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
        if(s_jelly_notification_dot != NULL) {
            lv_obj_add_flag(s_jelly_notification_dot, LV_OBJ_FLAG_HIDDEN);
        }
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
        s_second_visible = (s_selected_face == 0U) ? 1U : 0U;
        s_second_angle_valid = 0U;
        lv_obj_add_flag(s_pager, LV_OBJ_FLAG_SCROLLABLE);
        WatchFace_SetInfoAmbient(0U);
        WatchFace_SetJellyAmbient(0U);
        WatchFace_RefreshNotificationIndicator();
        if(s_selected_face == 0U) {
            WatchFace_UpdateSecond(NULL);
        }
        lv_obj_set_style_bg_color(s_center_dot,
                                  lv_color_hex(0xFF4D5AU),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_center_dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_center_dot, 0, LV_PART_MAIN);
    }

    lv_obj_invalidate(s_meter);
    lv_obj_invalidate(s_center_dot);
    WatchFace_ApplySelectedTimers();
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

static void WatchFace_PagerEvent(lv_event_t *event)
{
    lv_area_t analog_coords;
    lv_area_t info_coords;
    lv_area_t jelly_coords;
    int32_t analog_distance;
    int32_t info_distance;
    int32_t jelly_distance;

    (void)event;
    if((s_pager == NULL) ||
       (s_analog_page == NULL) ||
       (s_info_page == NULL) ||
       (s_jelly_page == NULL)) return;

    lv_obj_get_coords(s_analog_page, &analog_coords);
    lv_obj_get_coords(s_info_page, &info_coords);
    lv_obj_get_coords(s_jelly_page, &jelly_coords);
    analog_distance = analog_coords.x1;
    info_distance = info_coords.x1;
    jelly_distance = jelly_coords.x1;
    if(analog_distance < 0) analog_distance = -analog_distance;
    if(info_distance < 0) info_distance = -info_distance;
    if(jelly_distance < 0) jelly_distance = -jelly_distance;
    if((jelly_distance < analog_distance) &&
       (jelly_distance < info_distance)) {
        s_selected_face = WATCH_FACE_JELLY_INDEX;
    }
    else if(info_distance < analog_distance) {
        s_selected_face = WATCH_FACE_INFO_INDEX;
    }
    else {
        s_selected_face = 0U;
    }
    DeviceManager_SetWatchFace(s_selected_face);
    WatchFace_ApplyStatusBar();
    WatchFace_ApplySelectedTimers();
}

static lv_obj_t *WatchFace_CreateInfoArc(lv_obj_t *parent,
                                         lv_coord_t x,
                                         uint32_t color,
                                         int32_t min,
                                         int32_t max)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_size(arc, 50, 50);
    lv_obj_align(arc, LV_ALIGN_CENTER, x, 91);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_range(arc, min, max);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_value(arc, min);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x303740U), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    return arc;
}

static void WatchFace_CreateInfoHitArea(lv_obj_t *parent,
                                        lv_coord_t x,
                                        lv_coord_t y,
                                        lv_coord_t width,
                                        lv_coord_t height,
                                        AppUI_Page_t page)
{
    lv_obj_t *hit = lv_obj_create(parent);

    lv_obj_set_size(hit, width, height);
    lv_obj_align(hit, LV_ALIGN_CENTER, x, y);
    lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(hit, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hit, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(hit,
                        WatchFace_InfoClickEvent,
                        LV_EVENT_CLICKED,
                        (void *)(uintptr_t)page);
}

static void WatchFace_CreateInfo(lv_obj_t *parent)
{
    lv_obj_t *label;
    uint32_t i;
    static const lv_coord_t outline_x[8] = {-2, 2, 0, 0, -2, -2, 2, 2};
    static const lv_coord_t outline_y[8] = {0, 0, -2, 2, -2, 2, -2, 2};

    s_info_content = lv_obj_create(parent);
    lv_obj_set_size(s_info_content, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_info_content);
    lv_obj_clear_flag(s_info_content,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_info_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_content, 0, LV_PART_MAIN);

    s_info_date = lv_label_create(s_info_content);
    lv_label_set_text(s_info_date, "2000-01-01 SAT");
    lv_obj_set_style_text_font(s_info_date, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info_date, lv_color_hex(0xFF6745U), LV_PART_MAIN);
    lv_obj_align(s_info_date, LV_ALIGN_CENTER, -34, -111);

    s_info_time = lv_label_create(s_info_content);
    lv_label_set_text(s_info_time, "00:00");
    lv_obj_set_style_text_font(s_info_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info_time, lv_color_hex(0xF4F6F8U), LV_PART_MAIN);
    lv_obj_align(s_info_time, LV_ALIGN_CENTER, -31, -69);

    s_info_battery_arc = WatchFace_CreateInfoArc(s_info_content,
                                                  76,
                                                  0x19C819U,
                                                  0,
                                                  100);
    lv_obj_align(s_info_battery_arc, LV_ALIGN_CENTER, 76, -69);

    label = lv_label_create(s_info_content);
    lv_label_set_text(label, INFO_ICON_BATTERY);
    lv_obj_set_style_text_font(label, &ui_font_iconfont16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x19C819U), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 76, -77);

    s_info_battery_value = lv_label_create(s_info_content);
    lv_label_set_text(s_info_battery_value, "--%");
    lv_obj_set_style_text_font(s_info_battery_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info_battery_value, lv_color_hex(0xDDE3EAU), LV_PART_MAIN);
    lv_obj_align(s_info_battery_value, LV_ALIGN_CENTER, 76, -60);

    label = lv_label_create(s_info_content);
    lv_label_set_text(label, INFO_ICON_STEPS);
    lv_obj_set_style_text_font(label, &ui_font_iconfont24, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(INFO_COLOR_BLUE), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, -96, -13);

    label = lv_label_create(s_info_content);
    lv_label_set_text(label, "STEPS");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(INFO_COLOR_BLUE), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, -48, -13);

    s_info_steps_value = lv_label_create(s_info_content);
    lv_label_set_text(s_info_steps_value, "0");
    lv_obj_set_style_text_font(s_info_steps_value, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info_steps_value, lv_color_hex(0xF5F7FAU), LV_PART_MAIN);
    lv_obj_align(s_info_steps_value, LV_ALIGN_CENTER, -78, 17);

    s_info_steps_bar = lv_bar_create(s_info_content);
    lv_obj_set_size(s_info_steps_bar, 200, 9);
    lv_obj_align(s_info_steps_bar, LV_ALIGN_CENTER, 0, 43);
    lv_obj_clear_flag(s_info_steps_bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_bar_set_range(s_info_steps_bar, 0, 100);
    lv_bar_set_value(s_info_steps_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_info_steps_bar, lv_color_hex(0x202A38U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_info_steps_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_info_steps_bar, lv_color_hex(INFO_COLOR_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_info_steps_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    s_info_temp_arc = WatchFace_CreateInfoArc(s_info_content,
                                               -75,
                                               INFO_COLOR_ORANGE,
                                               -20,
                                               60);
    label = lv_label_create(s_info_content);
    lv_label_set_text(label, INFO_ICON_TEMPERATURE);
    lv_obj_set_style_text_font(label, &ui_font_iconfont30, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(INFO_COLOR_ORANGE), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, -75, 91);
    s_info_temp_value = lv_label_create(s_info_content);
    lv_label_set_text(s_info_temp_value, "-- C");
    lv_obj_set_style_text_font(s_info_temp_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_info_temp_value, LV_ALIGN_CENTER, -75, 126);

    s_info_humi_arc = WatchFace_CreateInfoArc(s_info_content,
                                               0,
                                               INFO_COLOR_CYAN,
                                               0,
                                               100);
    label = lv_label_create(s_info_content);
    lv_label_set_text(label, INFO_ICON_HUMIDITY);
    lv_obj_set_style_text_font(label, &ui_font_iconfont28, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(INFO_COLOR_CYAN), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 91);
    s_info_humi_value = lv_label_create(s_info_content);
    lv_label_set_text(s_info_humi_value, "--%");
    lv_obj_set_style_text_font(s_info_humi_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_info_humi_value, LV_ALIGN_CENTER, 0, 126);

    s_info_heart_arc = WatchFace_CreateInfoArc(s_info_content,
                                                75,
                                                INFO_COLOR_RED,
                                                0,
                                                200);
    label = lv_label_create(s_info_content);
    lv_label_set_text(label, MENU_ICON_HEART_RATE);
    lv_obj_set_style_text_font(label, &ui_font_iconfont34, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(INFO_COLOR_RED), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 75, 91);
    s_info_heart_value = lv_label_create(s_info_content);
    lv_label_set_text(s_info_heart_value, "--");
    lv_obj_set_style_text_font(s_info_heart_value, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_info_heart_value, LV_ALIGN_CENTER, 75, 126);

    WatchFace_CreateInfoHitArea(s_info_content, 76, -69, 58, 58,
                                APP_UI_PAGE_BATTERY);
    WatchFace_CreateInfoHitArea(s_info_content, 0, 15, 210, 72,
                                APP_UI_PAGE_MOTION);
    WatchFace_CreateInfoHitArea(s_info_content, -75, 104, 65, 75,
                                APP_UI_PAGE_ENVIRONMENT);
    WatchFace_CreateInfoHitArea(s_info_content, 0, 104, 65, 75,
                                APP_UI_PAGE_ENVIRONMENT);
    WatchFace_CreateInfoHitArea(s_info_content, 75, 104, 65, 75,
                                APP_UI_PAGE_HEART);

    s_info_ambient_group = lv_obj_create(parent);
    lv_obj_set_size(s_info_ambient_group, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_info_ambient_group);
    lv_obj_clear_flag(s_info_ambient_group,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_info_ambient_group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_ambient_group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_ambient_group, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_info_ambient_group, LV_OBJ_FLAG_HIDDEN);

    for(i = 0U; i < 8U; ++i) {
        s_info_ambient_labels[i] = lv_label_create(s_info_ambient_group);
        lv_label_set_text(s_info_ambient_labels[i], "00:00");
        lv_obj_set_style_text_font(s_info_ambient_labels[i],
                                   &lv_font_montserrat_48,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(s_info_ambient_labels[i],
                                    lv_color_hex(0xAAB1BAU),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_opa(s_info_ambient_labels[i], 90, LV_PART_MAIN);
        lv_obj_align(s_info_ambient_labels[i],
                     LV_ALIGN_CENTER,
                     outline_x[i],
                     outline_y[i]);
    }
    s_info_ambient_labels[8] = lv_label_create(s_info_ambient_group);
    lv_label_set_text(s_info_ambient_labels[8], "00:00");
    lv_obj_set_style_text_font(s_info_ambient_labels[8],
                               &lv_font_montserrat_48,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(s_info_ambient_labels[8],
                                lv_color_hex(0x05070AU),
                                LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_info_ambient_labels[8], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(s_info_ambient_labels[8]);
}

static void WatchFace_CreateJelly(lv_obj_t *parent)
{
    static const lv_coord_t outline_x[8] = {-3, 3, 0, 0, -3, -3, 3, 3};
    static const lv_coord_t outline_y[8] = {0, 0, -3, 3, -3, 3, -3, 3};
    static const lv_coord_t digit_x[4] = {-58, 58, -58, 58};
    static const lv_coord_t digit_y[4] = {-68, -68, 68, 68};
    static const uint32_t digit_color[4] = {
        0x79CEFFU, 0x79CEFFU, 0x8DE8A8U, 0x8DE8A8U
    };
    uint32_t digit;
    uint32_t layer;

    for(digit = 0U; digit < 4U; ++digit) {
        for(layer = 0U; layer < 9U; ++layer) {
            s_jelly_digit_labels[digit][layer] = lv_label_create(parent);
            lv_label_set_text(s_jelly_digit_labels[digit][layer], "0");
            lv_obj_set_style_text_font(s_jelly_digit_labels[digit][layer],
                                       &ui_font_Cuyuan135,
                                       LV_PART_MAIN);
            lv_obj_set_style_text_color(s_jelly_digit_labels[digit][layer],
                                        lv_color_hex(digit_color[digit]),
                                        LV_PART_MAIN);
            lv_obj_align(s_jelly_digit_labels[digit][layer],
                         LV_ALIGN_CENTER,
                         digit_x[digit] +
                         ((layer < 8U) ? outline_x[layer] : 0),
                         digit_y[digit] +
                         ((layer < 8U) ? outline_y[layer] : 0));
        }
    }

    s_jelly_notification_dot = lv_obj_create(parent);
    lv_obj_set_size(s_jelly_notification_dot, 9, 9);
    lv_obj_align(s_jelly_notification_dot, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_clear_flag(s_jelly_notification_dot,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_jelly_notification_dot,
                            LV_RADIUS_CIRCLE,
                            LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_jelly_notification_dot,
                              lv_color_hex(0xFF3038U),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_jelly_notification_dot,
                            LV_OPA_COVER,
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(s_jelly_notification_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_jelly_notification_dot, 0, LV_PART_MAIN);
}

static void WatchFace_ScheduleNextMinute(lv_timer_t *timer,
                                         const RTC_TimeTypeDef *time)
{
    uint32_t period;

    if((timer == NULL) || (time == NULL)) return;
    period = (uint32_t)(60U - time->Seconds) * 1000U;
    if(period < 250U) period = 60000U;
    lv_timer_set_period(timer, period);
}

static void WatchFace_UpdateJelly(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    static const lv_coord_t outline_x[8] = {-3, 3, 0, 0, -3, -3, 3, 3};
    static const lv_coord_t outline_y[8] = {0, 0, -3, 3, -3, 3, -3, 3};
    static const lv_coord_t digit_x[4] = {-58, 58, -58, 58};
    static const lv_coord_t digit_y[4] = {-68, -68, 68, 68};
    uint8_t values[4];
    uint32_t digit;
    uint32_t layer;

    if((s_jelly_page == NULL) ||
       (WatchFace_ReadRtc(&time, &date) == 0U)) return;

    values[0] = (uint8_t)(time.Hours / 10U);
    values[1] = (uint8_t)(time.Hours % 10U);
    values[2] = (uint8_t)(time.Minutes / 10U);
    values[3] = (uint8_t)(time.Minutes % 10U);

    if((time.Hours != s_jelly_last_hour) ||
       (time.Minutes != s_jelly_last_minute)) {
        s_jelly_last_hour = time.Hours;
        s_jelly_last_minute = time.Minutes;
        for(digit = 0U; digit < 4U; ++digit) {
            for(layer = 0U; layer < 9U; ++layer) {
                lv_label_set_text_fmt(s_jelly_digit_labels[digit][layer],
                                      "%u",
                                      (unsigned int)values[digit]);
                lv_obj_align(s_jelly_digit_labels[digit][layer],
                             LV_ALIGN_CENTER,
                             digit_x[digit] +
                             ((layer < 8U) ? outline_x[layer] : 0),
                             digit_y[digit] +
                             ((layer < 8U) ? outline_y[layer] : 0));
            }
        }
    }
    WatchFace_ScheduleNextMinute(timer, &time);
}

static void WatchFace_SetJellyAmbient(uint8_t enabled)
{
    uint32_t digit;
    uint32_t color;

    if(s_jelly_notification_dot == NULL) return;
    if(enabled != 0U) {
        lv_obj_add_flag(s_jelly_notification_dot, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        WatchFace_RefreshNotificationIndicator();
    }
    for(digit = 0U; digit < 4U; ++digit) {
        color = (enabled != 0U) ? 0x05070AU :
                ((digit < 2U) ? 0x79CEFFU : 0x8DE8A8U);
        lv_obj_set_style_text_color(s_jelly_digit_labels[digit][8],
                                    lv_color_hex(color),
                                    LV_PART_MAIN);
    }
    if(s_jelly_timer != NULL) {
        WatchFace_UpdateJelly(s_jelly_timer);
    }
}

static void WatchFace_InfoClickEvent(lv_event_t *event)
{
    AppUI_Page_t page;
    lv_indev_t *indev;

    if((AppUI_GetCurrentPage() != APP_UI_PAGE_WATCH) ||
       (AppUI_IsAmbient() != 0U)) return;

    page = (AppUI_Page_t)(uintptr_t)lv_event_get_user_data(event);
    indev = lv_indev_get_act();
    if(indev != NULL) lv_indev_wait_release(indev);
    AppUI_RequestPage(page);
}

static void WatchFace_UpdateInfoTime(const RTC_TimeTypeDef *time,
                                     const RTC_DateTypeDef *date)
{
    static const char * const weekdays[7] = {
        "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
    };
    const char *weekday = "---";
    uint32_t i;

    if((time == NULL) || (date == NULL) || (s_info_time == NULL)) return;

    if((date->WeekDay >= 1U) && (date->WeekDay <= 7U)) {
        weekday = weekdays[date->WeekDay - 1U];
    }
    if((time->Hours != s_info_last_hour) ||
       (time->Minutes != s_info_last_minute)) {
        s_info_last_hour = time->Hours;
        s_info_last_minute = time->Minutes;
        lv_label_set_text_fmt(s_info_time,
                              "%02u:%02u",
                              (unsigned int)time->Hours,
                              (unsigned int)time->Minutes);
        lv_obj_align(s_info_time, LV_ALIGN_CENTER, -31, -69);
        for(i = 0U; i < 9U; ++i) {
            if(s_info_ambient_labels[i] != NULL) {
                lv_label_set_text_fmt(s_info_ambient_labels[i],
                                      "%02u:%02u",
                                      (unsigned int)time->Hours,
                                      (unsigned int)time->Minutes);
            }
        }
    }
    if(date->Date != s_info_last_date) {
        s_info_last_date = date->Date;
        lv_label_set_text_fmt(s_info_date,
                              "20%02u-%02u-%02u %s",
                              (unsigned int)date->Year,
                              (unsigned int)date->Month,
                              (unsigned int)date->Date,
                              weekday);
        lv_obj_align(s_info_date, LV_ALIGN_CENTER, -34, -111);
    }
}

static void WatchFace_UpdateInfo(lv_timer_t *timer)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    const Battery_Data_t *battery;
    const Device_MotionData_t *motion;
    const Device_EnvironmentData_t *environment;
    const Device_HeartData_t *heart;
    int32_t value;
    int32_t progress;

    if((s_info_content == NULL) ||
       (WatchFace_ReadRtc(&time, &date) == 0U)) return;
    WatchFace_UpdateInfoTime(&time, &date);
    if(s_ambient != 0U) {
        WatchFace_ScheduleNextMinute(timer, &time);
        return;
    }

    battery = BatteryManager_Get();
    if((battery != NULL) &&
       ((battery->percent != s_info_last_battery) ||
        (battery->present != s_info_last_battery_present))) {
        s_info_last_battery = battery->percent;
        s_info_last_battery_present = battery->present;
        if(battery->present != 0U) {
            lv_label_set_text_fmt(s_info_battery_value,
                                  "%u%%",
                                  (unsigned int)battery->percent);
            lv_arc_set_value(s_info_battery_arc, battery->percent);
        }
        else {
            lv_label_set_text(s_info_battery_value, "EXT");
            lv_arc_set_value(s_info_battery_arc, 100);
        }
    }

    motion = DeviceManager_GetMotion();
    if((motion != NULL) && (motion->steps_today != s_info_last_steps)) {
        s_info_last_steps = motion->steps_today;
        lv_label_set_text_fmt(s_info_steps_value,
                              "%lu",
                              (unsigned long)motion->steps_today);
        progress = (int32_t)(motion->steps_today / 100U);
        if(progress > 100) progress = 100;
        lv_bar_set_value(s_info_steps_bar, progress, LV_ANIM_OFF);
    }

    environment = DeviceManager_GetEnvironment();
    if(environment != NULL) {
        value = (int32_t)(environment->temperature +
                          ((environment->temperature >= 0.0f) ? 0.5f : -0.5f));
        if((value != s_info_last_temp) ||
           (environment->connected != s_info_last_temp_connected)) {
            s_info_last_temp = (int16_t)value;
            s_info_last_temp_connected = environment->connected;
            if(environment->connected != 0U) {
                lv_label_set_text_fmt(s_info_temp_value, "%ld C", (long)value);
                if(value < -20) value = -20;
                if(value > 60) value = 60;
                lv_arc_set_value(s_info_temp_arc, value);
            }
            else {
                lv_label_set_text(s_info_temp_value, "-- C");
                lv_arc_set_value(s_info_temp_arc, -20);
            }
        }

        value = (int32_t)(environment->humidity + 0.5f);
        if((value != s_info_last_humi) ||
           (environment->connected != s_info_last_humi_connected)) {
            s_info_last_humi = (int16_t)value;
            s_info_last_humi_connected = environment->connected;
            if(environment->connected != 0U) {
                lv_label_set_text_fmt(s_info_humi_value, "%ld%%", (long)value);
                if(value < 0) value = 0;
                if(value > 100) value = 100;
                lv_arc_set_value(s_info_humi_arc, value);
            }
            else {
                lv_label_set_text(s_info_humi_value, "--%");
                lv_arc_set_value(s_info_humi_arc, 0);
            }
        }
    }

    heart = DeviceManager_GetHeart();
    if((heart != NULL) &&
       ((heart->bpm != s_info_last_heart) ||
        (heart->connected != s_info_last_heart_connected))) {
        s_info_last_heart = heart->bpm;
        s_info_last_heart_connected = heart->connected;
        if((heart->connected != 0U) && (heart->bpm != 0U)) {
            lv_label_set_text_fmt(s_info_heart_value,
                                  "%u",
                                  (unsigned int)heart->bpm);
            value = heart->bpm;
            if(value > 200) value = 200;
            lv_arc_set_value(s_info_heart_arc, value);
        }
        else {
            lv_label_set_text(s_info_heart_value, "--");
            lv_arc_set_value(s_info_heart_arc, 0);
        }
    }
}

static void WatchFace_SetInfoAmbient(uint8_t enabled)
{
    if((s_info_content == NULL) || (s_info_ambient_group == NULL)) return;

    if(enabled != 0U) {
        lv_obj_add_flag(s_info_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_info_ambient_group, LV_OBJ_FLAG_HIDDEN);
        WatchFace_UpdateInfo(s_info_timer);
    }
    else {
        lv_obj_add_flag(s_info_ambient_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_info_content, LV_OBJ_FLAG_HIDDEN);
        if(s_info_timer != NULL) lv_timer_set_period(s_info_timer,
                                                     WATCH_FACE_INFO_PERIOD_MS);
        WatchFace_UpdateInfo(NULL);
    }
}

static void WatchFace_ApplySelectedTimers(void)
{
    if(s_second_timer != NULL) {
        if((s_selected_face == 0U) && (s_ambient == 0U)) {
            s_second_visible = 1U;
            s_second_angle_valid = 0U;
            WatchFace_UpdateSecond(NULL);
            lv_timer_resume(s_second_timer);
            lv_timer_ready(s_second_timer);
        }
        else {
            if((s_second_angle_valid != 0U) && (s_second_visible != 0U)) {
                WatchFace_InvalidateSecond(s_second_angle_x100);
            }
            s_second_visible = 0U;
            lv_timer_pause(s_second_timer);
        }
    }

    if(s_info_timer != NULL) {
        if(s_selected_face == WATCH_FACE_INFO_INDEX) {
            lv_timer_resume(s_info_timer);
            lv_timer_ready(s_info_timer);
        }
        else {
            lv_timer_pause(s_info_timer);
        }
    }

    if(s_jelly_timer != NULL) {
        if(s_selected_face == WATCH_FACE_JELLY_INDEX) {
            WatchFace_UpdateJelly(s_jelly_timer);
            lv_timer_resume(s_jelly_timer);
            lv_timer_ready(s_jelly_timer);
        }
        else {
            lv_timer_pause(s_jelly_timer);
        }
    }
}

static void WatchFace_ApplyStatusBar(void)
{
    if(s_ambient != 0U) {
        StatusBar_SetVisible(0U);
        return;
    }
    if(s_selected_face == WATCH_FACE_JELLY_INDEX) {
        StatusBar_SetVisible(0U);
    }
    else {
        StatusBar_SetVisible(1U);
        StatusBar_SetBatteryVisible((s_selected_face == 0U) ? 1U : 0U);
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
    lv_area_t object_coords;
    uint16_t low_deg;
    uint16_t high_deg;
    uint32_t segment;
    uint32_t point;

    if(s_second_draw_obj == NULL) return;
    lv_obj_get_coords(s_second_draw_obj, &object_coords);

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
        area.x1 += object_coords.x1;
        area.x2 += object_coords.x1;
        area.y1 += object_coords.y1;
        area.y2 += object_coords.y1;
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
    lv_area_t object_coords;

    if((s_second_visible == 0U) || (s_second_angle_valid == 0U)) return;

    draw_ctx = lv_event_get_draw_ctx(event);
    if(draw_ctx == NULL) return;

    lv_obj_get_coords(s_second_draw_obj, &object_coords);
    center.x = object_coords.x1 + s_dial_center_x;
    center.y = object_coords.y1 + s_dial_center_y;
    WatchFace_GetInterpolatedSecondTip(s_second_angle_x100, &tip);
    tip.x += object_coords.x1;
    tip.y += object_coords.y1;

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
