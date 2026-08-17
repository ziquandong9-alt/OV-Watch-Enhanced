#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <stdint.h>

/* 固定容量避免动态内存；满时自动淘汰最旧通知。 */
#define NOTIFICATION_MAX_COUNT       12U
#define NOTIFICATION_TITLE_LENGTH    28U
#define NOTIFICATION_MESSAGE_LENGTH  112U

typedef enum {
    /* 类型只影响分类和配色，不把 UI 对象存进数据模型。 */
    NOTIFICATION_TYPE_HEART = 0,
    NOTIFICATION_TYPE_ENVIRONMENT,
    NOTIFICATION_TYPE_SEDENTARY,
    NOTIFICATION_TYPE_FALL,
    NOTIFICATION_TYPE_BATTERY,
    NOTIFICATION_TYPE_BLE
} Notification_Type_t;

typedef struct {
    /* id 在记录移动期间保持不变，用于事件 user_data 和详情定位。 */
    uint32_t id;
    Notification_Type_t type;
    uint8_t hour;
    uint8_t minute;
    char title[NOTIFICATION_TITLE_LENGTH];
    char message[NOTIFICATION_MESSAGE_LENGTH];
} Notification_Record_t;

void NotificationManager_Init(void);
uint8_t NotificationManager_Push(Notification_Type_t type,
                                 const char *title,
                                 const char *message);
uint8_t NotificationManager_GetCount(void);
/* 返回内部短期只读指针；下一次 Push/Remove 后应重新获取。 */
const Notification_Record_t *NotificationManager_Get(uint8_t index);
uint8_t NotificationManager_Remove(uint32_t id);
void NotificationManager_ClearAll(void);
/* 数据每变化一次递增，供 UI 进行廉价的变更检测。 */
uint32_t NotificationManager_GetGeneration(void);

#endif
