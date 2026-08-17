#include "stopwatch_page.h"

#include "app_ui.h"
#include "lvgl.h"
#include "stm32f4xx_hal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STOPWATCH_MAX_LAPS 8U

/* 运行中时间 = elapsed_before_start + (当前 tick - started_at)。 */
static uint8_t s_running;
static uint32_t s_elapsed_before_start;
static uint32_t s_started_at;
static uint32_t s_laps[STOPWATCH_MAX_LAPS];
static uint8_t s_lap_count;

static lv_timer_t *s_timer;
static lv_obj_t *s_time_label;
static lv_obj_t *s_laps_label;
static lv_obj_t *s_start_label;
static lv_obj_t *s_lap_label;

static uint32_t Stopwatch_Elapsed(void)
{
    /* HAL tick 无符号相减可跨回绕；暂停时直接返回累计值。 */
    if(s_running != 0U) {
        return s_elapsed_before_start + (uint32_t)(HAL_GetTick() - s_started_at);
    }
    return s_elapsed_before_start;
}

static void Stopwatch_Format(char *buffer, uint32_t size, uint32_t elapsed)
{
    /* 把毫秒拆成分、秒、百分秒；百分秒足以匹配屏幕刷新精度。 */
    uint32_t centiseconds = (elapsed / 10U) % 100U;
    uint32_t seconds = (elapsed / 1000U) % 60U;
    uint32_t minutes = elapsed / 60000U;
    (void)snprintf(buffer, size, "%02lu:%02lu.%02lu",
                   (unsigned long)minutes,
                   (unsigned long)seconds,
                   (unsigned long)centiseconds);
}

static void Stopwatch_UpdateLaps(void)
{
    /* 重建圈速文本到固定缓冲区，写入前始终检查剩余容量。 */
    char text[256];
    char segment_text[20];
    char total_text[20];
    uint32_t previous = 0U;
    uint32_t segment;
    uint32_t used = 0U;
    uint8_t index;

    text[0] = '\0';
    if(s_lap_count == 0U) {
        lv_label_set_text(s_laps_label, "No lap times");
        return;
    }

    for(index = 0U; index < s_lap_count; index++) {
        segment = s_laps[index] - previous;
        previous = s_laps[index];
        Stopwatch_Format(segment_text, sizeof(segment_text), segment);
        Stopwatch_Format(total_text, sizeof(total_text), s_laps[index]);
        used += (uint32_t)snprintf(&text[used], sizeof(text) - used,
                                   "L%u  %s  [%s]%s",
                                   (unsigned int)(index + 1U), segment_text,
                                   total_text,
                                   (index + 1U < s_lap_count) ? "\n" : "");
        if(used >= (sizeof(text) - 1U)) break;
    }
    lv_label_set_text(s_laps_label, text);
}

static void Stopwatch_Update(lv_timer_t *timer)
{
    /* timer 只刷新文字，不改变计时基准，因此偶尔丢帧不会造成时间误差。 */
    char text[24];
    (void)timer;
    Stopwatch_Format(text, sizeof(text), Stopwatch_Elapsed());
    lv_label_set_text(s_time_label, text);
}

static void Stopwatch_StartEvent(lv_event_t *event)
{
    /* Start/Stop 共用按钮：暂停时把本段时间折算进累计值。 */
    (void)event;
    AppUI_NotifyActivity();
    if(s_running != 0U) {
        s_elapsed_before_start = Stopwatch_Elapsed();
        s_running = 0U;
        lv_label_set_text(s_start_label, "START");
        lv_label_set_text(s_lap_label, "RESET");
    }
    else {
        s_started_at = HAL_GetTick();
        s_running = 1U;
        lv_label_set_text(s_start_label, "PAUSE");
        lv_label_set_text(s_lap_label, "LAP");
    }
    Stopwatch_Update(NULL);
}

static void Stopwatch_LapEvent(lv_event_t *event)
{
    /* 运行中记录圈速，停止时同一按钮承担 Reset。 */
    (void)event;
    AppUI_NotifyActivity();
    if(s_running != 0U) {
        if(s_lap_count < STOPWATCH_MAX_LAPS) {
            s_laps[s_lap_count++] = Stopwatch_Elapsed();
            Stopwatch_UpdateLaps();
        }
    }
    else {
        s_elapsed_before_start = 0U;
        s_lap_count = 0U;
        memset(s_laps, 0, sizeof(s_laps));
        Stopwatch_Update(NULL);
        Stopwatch_UpdateLaps();
    }
}

static lv_obj_t *Stopwatch_CreateButton(lv_obj_t *parent, const char *text,
                                        lv_color_t color, lv_event_cb_t callback,
                                        lv_obj_t **label_out)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;
    lv_obj_set_size(button, 102, 48);
    lv_obj_set_style_radius(button, 24, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(label);
    *label_out = label;
    return button;
}

void StopwatchPage_Create(void)
{
    /* 页面对象可重建，但计时状态用静态变量保留，返回页面仍能继续显示。 */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *lap_panel;
    lv_obj_t *start_button;
    lv_obj_t *lap_button;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "STOPWATCH");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58B7FFU), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    s_time_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, 48);

    lap_panel = lv_obj_create(screen);
    lv_obj_set_size(lap_panel, 218, 115);
    lv_obj_align(lap_panel, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_set_style_bg_color(lap_panel, lv_color_hex(0x10151BU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lap_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(lap_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(lap_panel, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lap_panel, 10, LV_PART_MAIN);
    lv_obj_set_scroll_dir(lap_panel, LV_DIR_VER);

    s_laps_label = lv_label_create(lap_panel);
    lv_obj_set_width(s_laps_label, 194);
    lv_obj_set_style_text_font(s_laps_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_laps_label, lv_color_hex(0xB8C0CAU), LV_PART_MAIN);
    lv_label_set_long_mode(s_laps_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_laps_label, LV_ALIGN_TOP_LEFT, 0, 0);

    start_button = Stopwatch_CreateButton(screen,
                                          (s_running != 0U) ? "PAUSE" : "START",
                                          lv_color_hex(0x167447U),
                                          Stopwatch_StartEvent, &s_start_label);
    lv_obj_align(start_button, LV_ALIGN_BOTTOM_LEFT, 10, -13);
    lap_button = Stopwatch_CreateButton(screen,
                                        (s_running != 0U) ? "LAP" : "RESET",
                                        lv_color_hex(0x3A414BU),
                                        Stopwatch_LapEvent, &s_lap_label);
    lv_obj_align(lap_button, LV_ALIGN_BOTTOM_RIGHT, -10, -13);

    Stopwatch_Update(NULL);
    Stopwatch_UpdateLaps();
    s_timer = lv_timer_create(Stopwatch_Update, 50U, NULL);
}

void StopwatchPage_Destroy(void)
{
    /* 删除显示 timer 不会停止逻辑计时；再次进入时通过 HAL tick 补算。 */
    if(s_timer != NULL) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    lv_obj_clean(lv_scr_act());
    s_time_label = NULL;
    s_laps_label = NULL;
    s_start_label = NULL;
    s_lap_label = NULL;
}
