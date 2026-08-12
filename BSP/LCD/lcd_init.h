#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "main.h"
#include "spi.h"
#include "tim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USE_HORIZONTAL              0U

#if (USE_HORIZONTAL == 0U) || (USE_HORIZONTAL == 1U)
#define LCD_W                       240U
#define LCD_H                       280U
#else
#define LCD_W                       280U
#define LCD_H                       240U
#endif

#define LCD_Y_OFFSET                20U
#define LCD_DEFAULT_BRIGHTNESS      50U
#define LCD_SPI_TIMEOUT_MS          100U

/*
 * 纯色填充每次准备多少行。默认 20 行，占 240x20x2 = 9600 bytes，
 * 240x280 全屏共 14 次 DMA。配合 lcd.c 的 32 位双像素填充降低准备开销。
 */
#define LCD_FILL_CHUNK_LINES        20U

#define LCD_RES_PORT                GPIOB
#define LCD_RES_PIN                 GPIO_PIN_7
#define LCD_DC_PORT                 GPIOB
#define LCD_DC_PIN                  GPIO_PIN_9
#define LCD_CS_PORT                 GPIOB
#define LCD_CS_PIN                  GPIO_PIN_8
#define LCD_BLK_PORT                GPIOB
#define LCD_BLK_PIN                 GPIO_PIN_0

#define LCD_RES_LOW()               HAL_GPIO_WritePin(LCD_RES_PORT, LCD_RES_PIN, GPIO_PIN_RESET)
#define LCD_RES_HIGH()              HAL_GPIO_WritePin(LCD_RES_PORT, LCD_RES_PIN, GPIO_PIN_SET)
#define LCD_DC_LOW()                HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_HIGH()               HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define LCD_CS_LOW()                HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_HIGH()               HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)

void LCD_GPIO_Init(void);
void LCD_Init(void);

void LCD_Writ_Bus(uint8_t data);
void LCD_WR_REG(uint8_t command);
void LCD_WR_DATA8(uint8_t data);
void LCD_WR_DATA(uint16_t data);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

void LCD_Set_Light(uint8_t percent);
void LCD_Open_Light(void);
void LCD_Close_Light(void);
void LCD_ST7789_SleepIn(void);
void LCD_ST7789_SleepOut(void);

#ifdef __cplusplus
}
#endif

#endif
