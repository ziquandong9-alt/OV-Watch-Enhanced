#include "history_manager.h"

#include "BL24C02.h"
#include "stm32f4xx_hal.h"

#include <string.h>

/* 24C02 的 0x90 起始区域保存最近 7 天，每天占两个 8 字节页。 */
#define HISTORY_BASE_ADDRESS  0x90U
#define HISTORY_RECORD_SIZE   16U
#define HISTORY_MARKER_A      0xD3U
#define HISTORY_MARKER_B      0x3DU

static History_Day_t s_days[HISTORY_MAX_DAYS];
static uint8_t s_sequences[HISTORY_MAX_DAYS];
static uint8_t s_count;
static uint8_t s_next_slot;
static uint8_t s_next_sequence;
static uint8_t s_eeprom_available;

static uint8_t Checksum8(const uint8_t *data)
{
    /* 用固定种子异或前 7 字节；目标是检测掉电/写坏，不是密码学安全。 */
    uint8_t value = 0x6BU;
    uint8_t i;
    for(i = 0U; i < 7U; i++) value ^= data[i];
    return value;
}

static uint8_t SequenceIsNewer(uint8_t candidate, uint8_t current)
{
    /* 半区间比较允许 8 位序号从 255 回绕到 0。 */
    return ((candidate != current) &&
            ((uint8_t)(candidate - current) < 128U)) ? 1U : 0U;
}

/* 扫描七个 EEPROM 槽并建立按序号排列的 RAM 历史缓存。 */
void HistoryManager_Init(uint8_t eeprom_available)
{
    uint8_t slot;
    uint8_t raw[HISTORY_RECORD_SIZE];
    uint8_t newest_slot = 0U;
    uint8_t newest_sequence = 0U;
    uint8_t have_newest = 0U;

    /* RAM 缓存先清零；EEPROM 不可用时页面仍可安全显示“无记录”。 */
    memset(s_days, 0, sizeof(s_days));
    memset(s_sequences, 0, sizeof(s_sequences));
    s_count = 0U;
    s_next_slot = 0U;
    s_next_sequence = 0U;
    s_eeprom_available = eeprom_available;
    if(eeprom_available == 0U) return;

    /* 扫描全部槽，只有双标记、双序号、双校验和都正确的记录才接收。 */
    for(slot = 0U; slot < HISTORY_MAX_DAYS; slot++) {
        BL24C02_Read((uint8_t)(HISTORY_BASE_ADDRESS + slot * HISTORY_RECORD_SIZE),
                     HISTORY_RECORD_SIZE, raw);
        if((raw[0] != HISTORY_MARKER_A) || (raw[8] != HISTORY_MARKER_B) ||
           (raw[1] != raw[9]) || (raw[7] != Checksum8(raw)) ||
           (raw[15] != Checksum8(&raw[8])) ||
           (raw[3] < 1U) || (raw[3] > 12U) ||
           (raw[4] < 1U) || (raw[4] > 31U)) continue;

        s_days[slot].valid = 1U;
        s_days[slot].year = raw[2];
        s_days[slot].month = raw[3];
        s_days[slot].day = raw[4];
        s_days[slot].steps = (uint16_t)raw[5] | ((uint16_t)raw[6] << 8);
        s_days[slot].average_heart_rate = raw[10];
        s_days[slot].temperature_c = (int8_t)raw[11];
        s_days[slot].humidity_percent = raw[12];
        s_sequences[slot] = raw[1];
        s_count++;
        /* 找到最新序号，下一次写入它后面的槽，形成循环日志。 */
        if((have_newest == 0U) ||
           (SequenceIsNewer(raw[1], newest_sequence) != 0U)) {
            newest_sequence = raw[1];
            newest_slot = slot;
            have_newest = 1U;
        }
    }
    if(have_newest != 0U) {
        s_next_slot = (uint8_t)((newest_slot + 1U) % HISTORY_MAX_DAYS);
        s_next_sequence = (uint8_t)(newest_sequence + 1U);
    }
}

/* 将一天汇总值以双页掉电安全格式写进下一循环槽。 */
void HistoryManager_RecordDay(uint8_t year, uint8_t month, uint8_t day,
                              uint32_t steps, uint16_t heart_rate,
                              float temperature, float humidity)
{
    uint8_t raw[HISTORY_RECORD_SIZE] = {0};
    uint8_t address;
    int16_t rounded_temperature;
    uint16_t clipped_steps;

    if(s_eeprom_available == 0U) return;
    /* EEPROM 格式只给步数 16 位，超出时饱和而不是截断回绕。 */
    clipped_steps = (steps > 65535UL) ? 65535U : (uint16_t)steps;
    rounded_temperature = (int16_t)((temperature >= 0.0f) ?
                                    (temperature + 0.5f) :
                                    (temperature - 0.5f));
    if(rounded_temperature > 127) rounded_temperature = 127;
    if(rounded_temperature < -128) rounded_temperature = -128;

    /* 前后两半使用不同 marker 和独立校验，可识别只写完一页的断电记录。 */
    raw[0] = HISTORY_MARKER_A;
    raw[1] = s_next_sequence;
    raw[2] = year;
    raw[3] = month;
    raw[4] = day;
    raw[5] = (uint8_t)clipped_steps;
    raw[6] = (uint8_t)(clipped_steps >> 8);
    raw[7] = Checksum8(raw);
    raw[8] = HISTORY_MARKER_B;
    raw[9] = s_next_sequence;
    raw[10] = (heart_rate > 255U) ? 255U : (uint8_t)heart_rate;
    raw[11] = (uint8_t)((int8_t)rounded_temperature);
    raw[12] = (humidity < 0.0f) ? 0U :
              ((humidity > 100.0f) ? 100U : (uint8_t)(humidity + 0.5f));
    raw[15] = Checksum8(&raw[8]);

    address = (uint8_t)(HISTORY_BASE_ADDRESS + s_next_slot * HISTORY_RECORD_SIZE);
    /* 先写负载页、后写头页；中途掉电时旧头与新负载序号不一致，会被拒绝。 */
    BL24C02_Write((uint8_t)(address + 8U), 8U, &raw[8]);
    HAL_Delay(6U);
    BL24C02_Write(address, 8U, raw);
    HAL_Delay(6U);

    /* EEPROM 成功写完后同步更新 RAM 镜像，页面无需再次读 I2C。 */
    s_days[s_next_slot].valid = 1U;
    s_days[s_next_slot].year = year;
    s_days[s_next_slot].month = month;
    s_days[s_next_slot].day = day;
    s_days[s_next_slot].steps = clipped_steps;
    s_days[s_next_slot].average_heart_rate = raw[10];
    s_days[s_next_slot].temperature_c = (int8_t)raw[11];
    s_days[s_next_slot].humidity_percent = raw[12];
    s_sequences[s_next_slot] = s_next_sequence;
    if(s_count < HISTORY_MAX_DAYS) s_count++;
    s_next_slot = (uint8_t)((s_next_slot + 1U) % HISTORY_MAX_DAYS);
    s_next_sequence++;
}

uint8_t HistoryManager_GetCount(void)
{
    /* 返回通过校验并已装入 RAM 的有效天数。 */
    return s_count;
}

uint8_t HistoryManager_GetNewest(uint8_t index, History_Day_t *day)
{
    uint8_t slot;
    if((day == NULL) || (index >= s_count)) return 0U;
    /* s_next_slot 指向下一次写入位置，向前倒推得到第 index 条最新记录。 */
    slot = (uint8_t)((s_next_slot + HISTORY_MAX_DAYS - 1U - index) %
                     HISTORY_MAX_DAYS);
    if(s_days[slot].valid == 0U) return 0U;
    *day = s_days[slot];
    return 1U;
}
