#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"

/* 心率算法使用的 7 元素固定环形队列，不进行 malloc。 */
#define QUEUE_SIZE 7

typedef struct {
    /* front/rear 是数组索引，size 是当前有效元素数。 */
    int8_t front;
    int8_t rear;
    int8_t size;
    uint32_t data[QUEUE_SIZE];
} Queue;

void initQueue(Queue *queue);
bool isQueueEmpty(Queue *queue);
bool isQueueFull(Queue *queue);
void enqueue(Queue *queue, unsigned long item);
uint32_t dequeue(Queue *queue);
