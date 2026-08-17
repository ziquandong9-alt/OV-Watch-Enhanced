#ifndef NOTIFICATION_PAGE_H
#define NOTIFICATION_PAGE_H

#include <stdint.h>

/* 创建通知列表；列表和详情作为同一 AppUI 页的内部状态。 */
void NotificationPage_Create(void);
/* 详情层消费返回时返回 1，列表层返回 0。 */
uint8_t NotificationPage_HandleBack(void);
void NotificationPage_Destroy(void);

#endif
