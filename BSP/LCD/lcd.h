#ifndef __LCD_H
#define __LCD_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*LCD_CallbackFunc_t)(void);

#define WHITE       0xFFFFU
#define BLACK       0x0000U
#define BLUE        0x001FU
#define RED         0xF800U
#define MAGENTA     0xF81FU
#define GREEN       0x07E0U
#define CYAN        0x7FFFU
#define YELLOW      0xFFE0U
#define BROWN       0xBC40U
#define BRRED       0xFC07U
#define GRAY        0x8430U

/*
 * 在主循环或 GUI 任务中周期调用：
 * 负责等待 SPI 物理空闲、启动下一段 DMA、执行完成通知。
 */
void LCD_Service(void);

/* 必须由 DMA2_Stream2_IRQHandler() 调用；函数内部不自旋等待。 */
void LCD_DMA_TX_IRQHandler(void);

/* 阻塞到完整链式 DMA 完成。不要在中断中调用。 */
void LCD_WaitForDMA(void);
uint8_t LCD_IsDMABusy(void);

/* 给 LVGL 使用：DMA 完成后回调，回调中应调用 lv_disp_flush_ready()。 */
void LCD_Set_Flush_Complete_Callback(LCD_CallbackFunc_t callback);

/*
 * 异步发送 RGB565 像素块。
 * pixels 在 DMA 完成回调前必须保持有效。
 * 驱动会自动将超过 65535 bytes 的连续缓冲拆成多个偶数字节 DMA 包。
 */
HAL_StatusTypeDef LCD_Color_Fill_DMA(uint16_t x1,
                                     uint16_t y1,
                                     uint16_t x2,
                                     uint16_t y2,
                                     const uint16_t *pixels);

/* 兼容原工程接口；内部启动异步 DMA。忙时不会等待，本次提交会被忽略。 */
void LCD_Color_Fill(uint16_t x1,
                    uint16_t y1,
                    uint16_t x2,
                    uint16_t y2,
                    uint16_t *pixels);

/* 纯色填充：使用可配置的多行静态缓冲分块 DMA，函数返回时已经完成。 */
void LCD_Fill(uint16_t x1,
              uint16_t y1,
              uint16_t x2,
              uint16_t y2,
              uint16_t color);

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1,
                  uint16_t y1,
                  uint16_t x2,
                  uint16_t y2,
                  uint16_t color);
void LCD_DrawRectangle(uint16_t x1,
                       uint16_t y1,
                       uint16_t x2,
                       uint16_t y2,
                       uint16_t color);
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t radius, uint16_t color);

void LCD_ShowChinese(uint16_t x, uint16_t y, uint8_t *text,
                     uint16_t foreground, uint16_t background,
                     uint8_t size, uint8_t overlay);
void LCD_ShowChinese12x12(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay);
void LCD_ShowChinese16x16(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay);
void LCD_ShowChinese24x24(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay);
void LCD_ShowChinese32x32(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay);

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t character,
                  uint16_t foreground, uint16_t background,
                  uint8_t size, uint8_t overlay);
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *text,
                    uint16_t foreground, uint16_t background,
                    uint8_t size, uint8_t overlay);
uint32_t mypow(uint8_t base, uint8_t exponent);
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t number,
                    uint8_t length, uint16_t foreground,
                    uint16_t background, uint8_t size);
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float number,
                       uint8_t length, uint16_t foreground,
                       uint16_t background, uint8_t size);
void LCD_ShowPicture(uint16_t x, uint16_t y,
                     uint16_t length, uint16_t width,
                     const uint8_t picture[]);

#ifdef __cplusplus
}
#endif

#endif
