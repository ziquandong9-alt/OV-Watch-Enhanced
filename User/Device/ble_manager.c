#include "ble_manager.h"

#include "KT6328.h"
#include "battery_manager.h"
#include "device_manager.h"
#include "notification_manager.h"
#include "rtc.h"
#include "usart.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DMA 每次接收 64 字节，再搬进 256 字节环形队列供主循环解析。 */
#define BLE_DMA_BUFFER_SIZE 64U
#define BLE_RING_SIZE       256U
#define BLE_LINE_SIZE       160U
#define BLE_LINK_TIMEOUT_MS 15000UL

static uint8_t s_dma_buffer[BLE_DMA_BUFFER_SIZE];
/* 环形队列由中断写、主循环读，故数据和索引必须声明为 volatile。 */
static volatile uint8_t s_ring[BLE_RING_SIZE];
static volatile uint16_t s_write_index;
static volatile uint16_t s_read_index;
static char s_line[BLE_LINE_SIZE];
static uint16_t s_line_length;
static uint8_t s_connected;
static uint8_t s_suspended;
static uint32_t s_last_packet_ms;
static uint32_t s_generation;

static uint8_t Weekday(uint16_t year, uint8_t month, uint8_t day)
{
    /* Sakamoto 算法：根据手机下发日期补齐 STM32 RTC 需要的星期字段。 */
    static const uint8_t table[] = {0U, 3U, 2U, 5U, 0U, 3U,
                                    5U, 1U, 4U, 6U, 2U, 4U};
    uint32_t y = year;
    uint32_t value;
    if(month < 3U) y--;
    value = (y + y / 4U - y / 100U + y / 400U +
             table[month - 1U] + day) % 7U;
    return (value == 0U) ? RTC_WEEKDAY_SUNDAY : (uint8_t)value;
}

static void StartReceive(void)
{
    /* ReceiveToIdle 能在收到不定长串口数据并出现空闲时立即回调。 */
    if(HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_dma_buffer,
                                   BLE_DMA_BUFFER_SIZE) == HAL_OK) {
        /* 不使用半传输事件，避免同一块 DMA 数据被拆成两次无意义回调。 */
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}

static void SetConnected(uint8_t connected)
{
    /* 归一化为严格的 0/1，且只在状态真正变化时增加 generation。 */
    connected = connected ? 1U : 0U;
    if(connected != s_connected) {
        s_connected = connected;
        s_generation++;
    }
}

static void ApplyTime(const char *payload)
{
    unsigned int year, month, day, hour, minute, second;
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    /* 协议格式：TIME,YY,MM,DD,hh,mm,ss；这里收到的是 TIME, 之后的负载。 */
    if(sscanf(payload, "%u,%u,%u,%u,%u,%u",
              &year, &month, &day, &hour, &minute, &second) != 6) return;
    /* 严格检查范围，禁止畸形串口数据写入 RTC 寄存器。 */
    if((year > 99U) || (month < 1U) || (month > 12U) ||
       (day < 1U) || (day > 31U) || (hour > 23U) ||
       (minute > 59U) || (second > 59U)) return;
    time.Hours = (uint8_t)hour;
    time.Minutes = (uint8_t)minute;
    time.Seconds = (uint8_t)second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    date.Year = (uint8_t)year;
    date.Month = (uint8_t)month;
    date.Date = (uint8_t)day;
    date.WeekDay = Weekday((uint16_t)(2000U + year),
                           (uint8_t)month, (uint8_t)day);
    if((HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) &&
       (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK)) {
        DeviceManager_SaveDateTimeNow();
    }
}

static void HandleLine(char *line)
{
    char *separator;
    /* 任何完整协议行都视作链路存活，并刷新 15 秒超时计时。 */
    s_last_packet_ms = HAL_GetTick();
    SetConnected(1U);
    if((strcmp(line, "PING") == 0) || (strcmp(line, "CONNECTED") == 0)) return;
    if(strcmp(line, "DISCONNECTED") == 0) { SetConnected(0U); return; }
    if(strcmp(line, "CLEAR") == 0) { NotificationManager_ClearAll(); return; }
    if(strncmp(line, "TIME,", 5U) == 0) { ApplyTime(&line[5]); return; }
    /* 通知协议为 NOTIFY,标题|正文；把分隔符原地改成 '\0' 可避免复制字符串。 */
    if(strncmp(line, "NOTIFY,", 7U) == 0) {
        separator = strchr(&line[7], '|');
        if(separator != NULL) {
            *separator = '\0';
            NotificationManager_Push(NOTIFICATION_TYPE_BLE, &line[7], separator + 1);
        }
    }
}

void BLEManager_Init(void)
{
    /* 初始化生产者/消费者索引后再启动 DMA，防止回调看到旧队列状态。 */
    s_write_index = 0U;
    s_read_index = 0U;
    s_line_length = 0U;
    s_connected = 0U;
    s_suspended = 0U;
    s_generation = 0U;
    KT6328_Init();
    StartReceive();
}

void BLEManager_Process(void)
{
    uint8_t byte;
    if(s_suspended != 0U) return;
    /* 中断只搬运字节，耗时的组帧和命令解析全部放在主循环。 */
    while(s_read_index != s_write_index) {
        byte = s_ring[s_read_index];
        s_read_index = (uint16_t)((s_read_index + 1U) % BLE_RING_SIZE);
        /* CR、LF 都可结束一行；连续 CRLF 不会生成空命令。 */
        if((byte == '\n') || (byte == '\r')) {
            if(s_line_length != 0U) {
                s_line[s_line_length] = '\0';
                HandleLine(s_line);
                s_line_length = 0U;
            }
        }
        else if(s_line_length < (BLE_LINE_SIZE - 1U)) s_line[s_line_length++] = (char)byte;
        else s_line_length = 0U; /* 超长帧直接丢弃，防止越界写。 */
    }
    if((s_connected != 0U) &&
       ((uint32_t)(HAL_GetTick() - s_last_packet_ms) >= BLE_LINK_TIMEOUT_MS)) SetConnected(0U);
}

void BLEManager_Suspend(void)
{
    /* STOP 前先终止 DMA 并关闭模块，避免 UART/DMA 阻止低功耗。 */
    s_suspended = 1U;
    (void)HAL_UART_AbortReceive(&huart1);
    KT6328_Disable();
    SetConnected(0U);
}

void BLEManager_Resume(void)
{
    /* 模块上电后重新启动 ReceiveToIdle；旧连接必须等待新数据确认。 */
    KT6328_Enable();
    s_suspended = 0U;
    StartReceive();
}

/* 轻量 Getter 不访问 UART，只读取协议层缓存。 */
uint8_t BLEManager_IsConnected(void) { return s_connected; }
/* generation 用于 UI 判断连接状态是否真实变化。 */
uint32_t BLEManager_GetGeneration(void) { return s_generation; }

void BLEManager_SendStatus(void)
{
    char message[96];
    const Battery_Data_t *battery = BatteryManager_Get();
    const Device_MotionData_t *motion = DeviceManager_GetMotion();
    /* snprintf 限制输出长度；只有结果完整落入缓冲区才允许发送。 */
    int length = snprintf(message, sizeof(message), "STATUS,%u,%lu,%u\r\n",
                          battery->percent, motion->steps_today,
                          BLEManager_IsConnected());
    if((length > 0) && (length < (int)sizeof(message)))
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)message, (uint16_t)length, 20U);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size)
{
    uint16_t i;
    uint16_t next;
    /* HAL 回调是全局入口，必须先确认事件确实来自蓝牙使用的 USART1。 */
    if(uart != &huart1) return;
    for(i = 0U; i < size; i++) {
        next = (uint16_t)((s_write_index + 1U) % BLE_RING_SIZE);
        /* next 追上 read 表示环形队列已满；保留旧数据并丢弃本批剩余字节。 */
        if(next == s_read_index) break;
        s_ring[s_write_index] = s_dma_buffer[i];
        s_write_index = next;
    }
    /* DMA 接收是一次性的，每次空闲事件后都要重新挂载下一次接收。 */
    if(s_suspended == 0U) StartReceive();
}
