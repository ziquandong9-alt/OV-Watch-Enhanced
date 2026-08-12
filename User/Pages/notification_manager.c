#include "notification_manager.h"

#include "rtc.h"
#include "stm32f4xx_hal.h"

#include <string.h>

static Notification_Record_t s_records[NOTIFICATION_MAX_COUNT];
static uint8_t s_count;
static uint32_t s_next_id = 1U;
static uint32_t s_generation = 1U;

static void CopyText(char *destination, uint32_t capacity, const char *source)
{
    if((destination == NULL) || (capacity == 0U)) return;
    if(source == NULL) source = "";
    (void)strncpy(destination, source, capacity - 1U);
    destination[capacity - 1U] = '\0';
}

void NotificationManager_Init(void)
{
    memset(s_records, 0, sizeof(s_records));
    s_count = 0U;
    s_next_id = 1U;
    s_generation++;
}

uint8_t NotificationManager_Push(Notification_Type_t type,
                                 const char *title,
                                 const char *message)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint8_t index;

    if((title == NULL) || (message == NULL)) return 0U;

    if(s_count < NOTIFICATION_MAX_COUNT) s_count++;
    for(index = s_count - 1U; index > 0U; index--) {
        s_records[index] = s_records[index - 1U];
    }

    memset(&s_records[0], 0, sizeof(s_records[0]));
    s_records[0].id = s_next_id++;
    if(s_next_id == 0U) s_next_id = 1U;
    s_records[0].type = type;
    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) {
        (void)HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
        s_records[0].hour = time.Hours;
        s_records[0].minute = time.Minutes;
    }
    CopyText(s_records[0].title, NOTIFICATION_TITLE_LENGTH, title);
    CopyText(s_records[0].message, NOTIFICATION_MESSAGE_LENGTH, message);
    s_generation++;
    return 1U;
}

uint8_t NotificationManager_GetCount(void)
{
    return s_count;
}

const Notification_Record_t *NotificationManager_Get(uint8_t index)
{
    if(index >= s_count) return NULL;
    return &s_records[index];
}

uint8_t NotificationManager_Remove(uint32_t id)
{
    uint8_t index;
    uint8_t move;

    for(index = 0U; index < s_count; index++) {
        if(s_records[index].id == id) {
            for(move = index; (move + 1U) < s_count; move++) {
                s_records[move] = s_records[move + 1U];
            }
            s_count--;
            memset(&s_records[s_count], 0, sizeof(s_records[s_count]));
            s_generation++;
            return 1U;
        }
    }
    return 0U;
}

void NotificationManager_ClearAll(void)
{
    if(s_count == 0U) return;
    memset(s_records, 0, sizeof(s_records));
    s_count = 0U;
    s_generation++;
}

uint32_t NotificationManager_GetGeneration(void)
{
    return s_generation;
}
