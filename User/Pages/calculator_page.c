#include "calculator_page.h"

#include "app_ui.h"
#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALC_INPUT_SIZE 20U

/* 计算器采用“累加器 + 待执行运算符 + 当前输入”的简单状态机。 */
static lv_obj_t *s_display;
static lv_obj_t *s_keypad;
static char s_input[CALC_INPUT_SIZE];
static double s_accumulator;
static char s_pending_operator;
static uint8_t s_start_new_input;
static uint8_t s_error;

static const char *s_key_map[] = {
    "C", "+/-", "%", "/", "\n",
    "7", "8", "9", "*", "\n",
    "4", "5", "6", "-", "\n",
    "1", "2", "3", "+", "\n",
    "0", ".", "DEL", "=", ""
};

static void Calculator_Reset(void)
{
    /* AC 操作把显示和所有运算状态恢复到确定的初始值。 */
    strcpy(s_input, "0");
    s_accumulator = 0.0;
    s_pending_operator = 0;
    s_start_new_input = 1U;
    s_error = 0U;
}

static void Calculator_ShowInput(void)
{
    /* 输入缓冲区始终保持 NUL 结尾，可直接交给 LVGL label。 */
    lv_label_set_text(s_display, s_input);
}

static void Calculator_ShowValue(double value)
{
    /* 将浮点结果格式化到固定长度显示区，并处理超长/不可表示情况。 */
    if((value > 999999999999.0) || (value < -999999999999.0)) {
        strcpy(s_input, "OVERFLOW");
        s_error = 1U;
    }
    else {
        (void)snprintf(s_input, sizeof(s_input), "%.10g", value);
    }
    Calculator_ShowInput();
}

static uint8_t Calculator_Apply(double right)
{
    /* 把 accumulator pending_operator right 归约成新 accumulator。 */
    switch(s_pending_operator) {
    case '+': s_accumulator += right; break;
    case '-': s_accumulator -= right; break;
    case '*': s_accumulator *= right; break;
    case '/':
        if((right > -0.0000000001) && (right < 0.0000000001)) {
            strcpy(s_input, "DIV BY ZERO");
            s_error = 1U;
            Calculator_ShowInput();
            return 0U;
        }
        s_accumulator /= right;
        break;
    default:
        s_accumulator = right;
        break;
    }
    return 1U;
}

static void Calculator_Append(const char *key)
{
    /* 输入长度始终保留一个字节给 '\0'，避免按键连击越界。 */
    size_t length;

    if((s_start_new_input != 0U) || (s_error != 0U)) {
        s_input[0] = '\0';
        s_start_new_input = 0U;
        s_error = 0U;
    }

    length = strlen(s_input);
    if(strcmp(key, ".") == 0) {
        if(strchr(s_input, '.') != NULL) return;
        if(length == 0U) {
            strcpy(s_input, "0.");
        }
        else if(length < (CALC_INPUT_SIZE - 1U)) {
            strcat(s_input, ".");
        }
    }
    else if(length < (CALC_INPUT_SIZE - 1U)) {
        strcat(s_input, key);
    }
    Calculator_ShowInput();
}

static void Calculator_KeyEvent(lv_event_t *event)
{
    /* btnmatrix 在 VALUE_CHANGED 时提供按键文本，据此推进计算状态机。 */
    uint16_t selected;
    const char *key;
    double value;
    size_t length;

    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    selected = lv_btnmatrix_get_selected_btn(s_keypad);
    key = lv_btnmatrix_get_btn_text(s_keypad, selected);
    if(key == NULL) return;

    /* 连续计算属于有效操作，应重新计算自动息屏时间。 */
    AppUI_NotifyActivity();
    if((key[0] >= '0') && (key[0] <= '9') && (key[1] == '\0')) {
        Calculator_Append(key);
    }
    else if(strcmp(key, ".") == 0) {
        Calculator_Append(key);
    }
    else if(strcmp(key, "C") == 0) {
        Calculator_Reset();
        Calculator_ShowInput();
    }
    else if(strcmp(key, "DEL") == 0) {
        if((s_error != 0U) || (s_start_new_input != 0U)) {
            strcpy(s_input, "0");
            s_error = 0U;
            s_start_new_input = 1U;
        }
        else {
            length = strlen(s_input);
            if(length > 0U) s_input[length - 1U] = '\0';
            if((s_input[0] == '\0') || (strcmp(s_input, "-") == 0)) {
                strcpy(s_input, "0");
                s_start_new_input = 1U;
            }
        }
        Calculator_ShowInput();
    }
    else if(strcmp(key, "+/-") == 0) {
        if((s_error == 0U) && (strcmp(s_input, "0") != 0)) {
            if(s_input[0] == '-') {
                memmove(s_input, &s_input[1], strlen(s_input));
            }
            else if(strlen(s_input) < (CALC_INPUT_SIZE - 1U)) {
                memmove(&s_input[1], s_input, strlen(s_input) + 1U);
                s_input[0] = '-';
            }
            Calculator_ShowInput();
        }
    }
    else if(strcmp(key, "%") == 0) {
        if(s_error == 0U) {
            value = strtod(s_input, NULL) / 100.0;
            Calculator_ShowValue(value);
            /* Keep it as the current operand so "50 + 10 % =" can complete. */
            s_start_new_input = 0U;
        }
    }
    else if((strcmp(key, "+") == 0) || (strcmp(key, "-") == 0) ||
            (strcmp(key, "*") == 0) || (strcmp(key, "/") == 0)) {
        if(s_error != 0U) return;
        if(s_start_new_input == 0U) {
            value = strtod(s_input, NULL);
            if(Calculator_Apply(value) == 0U) return;
            Calculator_ShowValue(s_accumulator);
        }
        else if(s_pending_operator == 0) {
            s_accumulator = strtod(s_input, NULL);
        }
        s_pending_operator = key[0];
        s_start_new_input = 1U;
    }
    else if(strcmp(key, "=") == 0) {
        if((s_error == 0U) && (s_pending_operator != 0) &&
           (s_start_new_input == 0U)) {
            value = strtod(s_input, NULL);
            if(Calculator_Apply(value) != 0U) {
                Calculator_ShowValue(s_accumulator);
                s_pending_operator = 0;
                s_start_new_input = 1U;
            }
        }
    }
}

void CalculatorPage_Create(void)
{
    /* 键盘由 lv_btnmatrix 一次创建，比 20 个独立按钮更省对象和 RAM。 */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;

    Calculator_Reset();
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "CALCULATOR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x7E8794U), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    s_display = lv_label_create(screen);
    lv_obj_set_width(s_display, 216);
    lv_obj_set_style_text_font(s_display, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_display, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_display, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(s_display, LV_ALIGN_TOP_MID, 0, 30);
    Calculator_ShowInput();

    s_keypad = lv_btnmatrix_create(screen);
    lv_btnmatrix_set_map(s_keypad, s_key_map);
    lv_obj_set_size(s_keypad, 228, 202);
    lv_obj_align(s_keypad, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(s_keypad, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_keypad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_keypad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_keypad, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_keypad, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_keypad, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_keypad, lv_color_hex(0x20252CU), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_keypad, lv_color_hex(0xF39A2EU),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(s_keypad, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_keypad, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_keypad, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_keypad, 10, LV_PART_ITEMS);
    lv_obj_add_event_cb(s_keypad, Calculator_KeyEvent, LV_EVENT_VALUE_CHANGED, NULL);
}

void CalculatorPage_Destroy(void)
{
    /* 本页没有后台 timer，清空屏幕和静态对象指针即可。 */
    lv_obj_clean(lv_scr_act());
    s_display = NULL;
    s_keypad = NULL;
}
