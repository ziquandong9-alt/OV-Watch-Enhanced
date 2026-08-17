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
    /* 有界复制并强制补 '\0'，蓝牙下发的长文本不能写穿记录结构。 */
    if((destination == NULL) || (capacity == 0U)) return;
    if(source == NULL) source = "";
    (void)strncpy(destination, source, capacity - 1U);
    destination[capacity - 1U] = '\0';
}

void NotificationManager_Init(void)
{
    /* 通知仅保存在 RAM；重启后清空，id 从 1 重新开始。 */
    memset(s_records, 0, sizeof(s_records));
    s_count = 0U;
    s_next_id = 1U;
    s_generation++;
}

/* 把新通知插到数组首位；满容量时淘汰最旧记录。 */
uint8_t NotificationManager_Push(Notification_Type_t type,
                                 const char *title,
                                 const char *message)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint8_t index;

    if((title == NULL) || (message == NULL)) return 0U;

    /* 新通知总放在下标 0；满 12 条时最末尾的旧通知自然被覆盖。 */
    if(s_count < NOTIFICATION_MAX_COUNT) s_count++;
    for(index = s_count - 1U; index > 0U; index--) {
        s_records[index] = s_records[index - 1U];
    }

    memset(&s_records[0], 0, sizeof(s_records[0]));
    /* id 独立于数组位置，删除导致记录移动后详情页仍能按 id 定位。 */
    s_records[0].id = s_next_id++;
    if(s_next_id == 0U) s_next_id = 1U;
    s_records[0].type = type;
    /* 时间读取成功才填写时间戳；读 Time 后仍按 STM32 要求补读 Date。 */
    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) {
        (void)HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
        s_records[0].hour = time.Hours;
        s_records[0].minute = time.Minutes;
    }
    CopyText(s_records[0].title, NOTIFICATION_TITLE_LENGTH, title);
    CopyText(s_records[0].message, NOTIFICATION_MESSAGE_LENGTH, message);
    /* 任何可见变化都递增 generation，页面据此决定是否需要重建。 */
    s_generation++;
    return 1U;
}

uint8_t NotificationManager_GetCount(void)
{
    /* 固定数组中的有效记录始终占据 [0, count)。 */
    return s_count;
}

const Notification_Record_t *NotificationManager_Get(uint8_t index)
{
    /* 返回内部只读记录；数组移动后旧指针可能失效，调用方不可长期保存。 */
    if(index >= s_count) return NULL;
    return &s_records[index];
}

uint8_t NotificationManager_Remove(uint32_t id)
{
    /* 按稳定 id 查找，删除后把后续记录前移以维持“最新在前”。 */
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
    /* 已为空时不递增 generation，避免 UI 做一次无意义重建。 */
    if(s_count == 0U) return;
    memset(s_records, 0, sizeof(s_records));
    s_count = 0U;
    s_generation++;
}

uint32_t NotificationManager_GetGeneration(void)
{
    /* 无副作用读取，适合页面 timer 高频比较。 */
    return s_generation;
}
