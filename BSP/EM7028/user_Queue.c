#include "user_Queue.h"


/* 固定容量环形队列：front 指向下一次出队，rear 指向最后一次入队。 */
void initQueue(Queue *queue) 
{
    /* rear=-1 使第一次 (rear+1)%SIZE 正好落到下标 0。 */
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
}

/* size 独立计数，避免 front==rear 时无法区分空和满。 */
bool isQueueEmpty(Queue *queue) 
{
    return queue->size == 0;
}

// 判断队列是否已满
bool isQueueFull(Queue *queue) 
{
    return queue->size == QUEUE_SIZE;
}

/* 从 rear 后一个槽写入；满队列时保留原数据并拒绝新值。 */
void enqueue(Queue *queue, unsigned long item) 
{
    if (isQueueFull(queue)) 
    {
        printf("队列已满，无法入队！\n");
        return;
    }
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    queue->data[queue->rear] = item;
    queue->size++;
}

/* 读取 front 后循环前移；空队列以 0 作为错误返回值。 */
uint32_t dequeue(Queue *queue) 
{
    if (isQueueEmpty(queue)) 
    {
        printf("队列为空，无法出队！\n");
        return 0;
    }
    unsigned long item = queue->data[queue->front];
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    queue->size--;
    return item;
}

