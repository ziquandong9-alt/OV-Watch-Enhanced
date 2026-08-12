#ifndef __LCD_FPS_TEST_H
#define __LCD_FPS_TEST_H

#include "main.h"

typedef struct
{
    uint32_t frames;
    uint32_t elapsed_ms;
    uint32_t fps_integer;
    uint32_t fps_fraction;
} LCD_FPS_Result_t;

extern volatile LCD_FPS_Result_t g_lcd_fps_result;

/* 测试全屏纯色刷新吞吐，duration_ms 建议 3000~10000。 */
void LCD_FPS_Test(uint32_t duration_ms);

#endif
