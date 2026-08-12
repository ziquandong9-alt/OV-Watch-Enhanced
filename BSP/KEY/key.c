#include "key.h"

#define KEY_SCAN_PERIOD_MS       10U
#define KEY_DEBOUNCE_TIME_MS     30U

static uint32_t s_last_scan_tick;
static uint32_t s_raw_change_tick;
static uint8_t s_raw_pressed;
static uint8_t s_stable_pressed;
static Key_Event_t s_pending_event;
static uint8_t s_ignore_until_release;

static uint8_t Key1_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

void Key_Init(void)
{
    uint32_t now = HAL_GetTick();

    s_raw_pressed = Key1_IsPressed();
    s_stable_pressed = s_raw_pressed;
    s_last_scan_tick = now;
    s_raw_change_tick = now;
    s_pending_event = KEY_EVENT_NONE;
    s_ignore_until_release = 0U;
}

void Key_Proc(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t current_pressed;

    if((now - s_last_scan_tick) < KEY_SCAN_PERIOD_MS) {
        return;
    }
    s_last_scan_tick = now;

    current_pressed = Key1_IsPressed();

    if(s_ignore_until_release != 0U) {
        if(current_pressed == 0U) {
            s_ignore_until_release = 0U;
            s_raw_pressed = 0U;
            s_stable_pressed = 0U;
            s_raw_change_tick = now;
        }
        return;
    }

    /* Restart debounce timing whenever the raw electrical level changes. */
    if(current_pressed != s_raw_pressed) {
        s_raw_pressed = current_pressed;
        s_raw_change_tick = now;
        return;
    }

    /* Accept a new stable level only after it remains unchanged for 30 ms. */
    if((current_pressed != s_stable_pressed) &&
       ((now - s_raw_change_tick) >= KEY_DEBOUNCE_TIME_MS)) {
        s_stable_pressed = current_pressed;

        /* Generate only the press edge. Holding the key never auto-repeats. */
        if((s_stable_pressed != 0U) && (s_pending_event == KEY_EVENT_NONE)) {
            s_pending_event = KEY_EVENT_KEY1_PRESSED;
        }
    }
}

Key_Event_t Key_GetEvent(void)
{
    Key_Event_t event = s_pending_event;

    s_pending_event = KEY_EVENT_NONE;
    return event;
}

void Key_IgnoreUntilRelease(void)
{
    s_ignore_until_release = 1U;
    s_pending_event = KEY_EVENT_NONE;
}
