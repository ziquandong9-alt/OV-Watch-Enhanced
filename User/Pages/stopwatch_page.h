#ifndef STOPWATCH_PAGE_H
#define STOPWATCH_PAGE_H

/* 创建秒表视图；计时状态在页面对象销毁后仍由模块静态变量保留。 */
void StopwatchPage_Create(void);
void StopwatchPage_Destroy(void);

#endif /* STOPWATCH_PAGE_H */
