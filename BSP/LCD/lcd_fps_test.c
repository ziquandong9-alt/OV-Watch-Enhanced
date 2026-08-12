#include "lcd_fps_test.h"
#include "lcd.h"
#include "lcd_init.h"
#include <stdio.h>

volatile LCD_FPS_Result_t g_lcd_fps_result = {0U, 0U, 0U, 0U};

void LCD_FPS_Test(uint32_t duration_ms)
{
    static const uint16_t colors[] = {
        RED, GREEN, BLUE, WHITE, BLACK, YELLOW, CYAN, MAGENTA
    };
    char text[32];
    uint32_t start_ms;
    uint32_t elapsed_ms;
    uint32_t frames = 0U;
    uint32_t fps_x100;

    if (duration_ms < 1000U)
    {
        duration_ms = 1000U;
    }

    start_ms = HAL_GetTick();

    do
    {
        LCD_Fill(0U,
                 0U,
                 LCD_W,
                 LCD_H,
                 colors[frames % (sizeof(colors) / sizeof(colors[0]))]);

        ++frames;
        elapsed_ms = HAL_GetTick() - start_ms;
    }
    while (elapsed_ms < duration_ms);

    /* fps_x100 避免 printf 浮点支持。 */
    fps_x100 = (elapsed_ms != 0U)
                  ? (frames * 100000U) / elapsed_ms
                  : 0U;

    g_lcd_fps_result.frames = frames;
    g_lcd_fps_result.elapsed_ms = elapsed_ms;
    g_lcd_fps_result.fps_integer = fps_x100 / 100U;
    g_lcd_fps_result.fps_fraction = fps_x100 % 100U;

    LCD_Fill(0U, 0U, LCD_W, LCD_H, BLACK);

    (void)sprintf(text,
                  "FPS:%lu.%02lu",
                  (unsigned long)g_lcd_fps_result.fps_integer,
                  (unsigned long)g_lcd_fps_result.fps_fraction);

    LCD_ShowString(20U,
                   LCD_H / 4U,
                   (const uint8_t *)text,
                   WHITE,
                   BLACK,
                   24U,
                   0U);
}
