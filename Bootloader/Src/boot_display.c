#include "boot_display.h"

#include "font8x8_basic.h"
#include "stm32f4xx_hal.h"

#include <stddef.h>
#include <string.h>

#define BOOT_LCD_WIDTH          240U
#define BOOT_LCD_HEIGHT         280U
#define BOOT_COLOR_BLACK        0x0000U
#define BOOT_COLOR_WHITE        0xFFFFU
#define BOOT_COLOR_CYAN         0x4E7FU
#define BOOT_COLOR_BLUE         0x2B5FU
#define BOOT_COLOR_GREEN        0x5F2DU
#define BOOT_COLOR_RED          0xF986U
#define BOOT_COLOR_GRAY         0x8410U
#define BOOT_COLOR_DARK_GRAY    0x2104U

#define LCD_RST_PIN             GPIO_PIN_7
#define LCD_CS_PIN              GPIO_PIN_8
#define LCD_DC_PIN              GPIO_PIN_9
#define LCD_BL_PIN              GPIO_PIN_0

static uint8_t s_character_pixels[24U * 24U * 2U];
static uint16_t s_startup_progress_width;
static uint16_t s_ota_progress_width;

static void LcdSelect(uint8_t selected)
{
    HAL_GPIO_WritePin(GPIOB, LCD_CS_PIN,
                     (selected != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void LcdWrite(const uint8_t *data, uint16_t length)
{
    while(length-- != 0U) {
        while((SPI1->SR & SPI_SR_TXE) == 0U) { }
        *(__IO uint8_t *)&SPI1->DR = *data++;
        while((SPI1->SR & SPI_SR_RXNE) == 0U) { }
        (void)*(__IO uint8_t *)&SPI1->DR;
    }
    while((SPI1->SR & SPI_SR_BSY) != 0U) { }
}

static void LcdCommand(uint8_t command)
{
    HAL_GPIO_WritePin(GPIOB, LCD_DC_PIN, GPIO_PIN_RESET);
    LcdWrite(&command, 1U);
    HAL_GPIO_WritePin(GPIOB, LCD_DC_PIN, GPIO_PIN_SET);
}

static void LcdData(const uint8_t *data, uint16_t length)
{
    HAL_GPIO_WritePin(GPIOB, LCD_DC_PIN, GPIO_PIN_SET);
    LcdWrite(data, length);
}

static void LcdCommandData(uint8_t command, const uint8_t *data, uint16_t length)
{
    LcdCommand(command);
    if((data != NULL) && (length != 0U)) LcdData(data, length);
}

static void LcdSetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t coordinates[4];
    uint16_t panel_y1 = y1;
    uint16_t panel_y2 = y2;

    coordinates[0] = (uint8_t)(x1 >> 8U);
    coordinates[1] = (uint8_t)x1;
    coordinates[2] = (uint8_t)(x2 >> 8U);
    coordinates[3] = (uint8_t)x2;
    LcdCommandData(0x2AU, coordinates, sizeof(coordinates));
    coordinates[0] = (uint8_t)(panel_y1 >> 8U);
    coordinates[1] = (uint8_t)panel_y1;
    coordinates[2] = (uint8_t)(panel_y2 >> 8U);
    coordinates[3] = (uint8_t)panel_y2;
    LcdCommandData(0x2BU, coordinates, sizeof(coordinates));
    LcdCommand(0x2CU);
}

static void FillRect(uint16_t x, uint16_t y, uint16_t width,
                     uint16_t height, uint16_t color)
{
    uint8_t pixels[128];
    uint32_t remaining;
    uint16_t index;

    if((width == 0U) || (height == 0U) ||
       (x >= BOOT_LCD_WIDTH) || (y >= BOOT_LCD_HEIGHT)) return;
    if((uint32_t)x + width > BOOT_LCD_WIDTH) width = (uint16_t)(BOOT_LCD_WIDTH - x);
    if((uint32_t)y + height > BOOT_LCD_HEIGHT) height = (uint16_t)(BOOT_LCD_HEIGHT - y);
    for(index = 0U; index < sizeof(pixels); index += 2U) {
        pixels[index] = (uint8_t)(color >> 8U);
        pixels[index + 1U] = (uint8_t)color;
    }
    LcdSetWindow(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
    remaining = (uint32_t)width * height;
    while(remaining != 0U) {
        uint16_t count = (remaining > 64U) ? 64U : (uint16_t)remaining;
        LcdData(pixels, (uint16_t)(count * 2U));
        remaining -= count;
    }
}

static void DrawFrame(uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, uint16_t thickness, uint16_t color)
{
    FillRect(x, y, width, thickness, color);
    FillRect(x, (uint16_t)(y + height - thickness), width, thickness, color);
    FillRect(x, y, thickness, height, color);
    FillRect((uint16_t)(x + width - thickness), y, thickness, height, color);
}

static void DrawCharacter(uint16_t x, uint16_t y, char character,
                          uint8_t scale, uint16_t foreground,
                          uint16_t background)
{
    uint16_t width = (uint16_t)(8U * scale);
    uint16_t height = width;
    uint16_t target_x;
    uint16_t target_y;
    uint32_t output = 0U;
    const uint8_t *glyph;

    if(((uint8_t)character >= 128U) || (scale == 0U) || (scale > 3U)) return;
    glyph = lcd_font8x8_basic[(uint8_t)character];
    for(target_y = 0U; target_y < height; ++target_y) {
        uint8_t row = glyph[target_y / scale];
        for(target_x = 0U; target_x < width; ++target_x) {
            uint16_t color = ((row & (uint8_t)(1U << (target_x / scale))) != 0U)
                                 ? foreground : background;
            s_character_pixels[output++] = (uint8_t)(color >> 8U);
            s_character_pixels[output++] = (uint8_t)color;
        }
    }
    LcdSetWindow(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
    LcdData(s_character_pixels, (uint16_t)output);
}

static void DrawTextCentered(uint16_t y, const char *text, uint8_t scale,
                             uint16_t foreground, uint16_t background)
{
    size_t length;
    uint16_t character_width = (uint16_t)(8U * scale);
    uint16_t x;

    if(text == NULL) return;
    length = strlen(text);
    if(length * character_width > BOOT_LCD_WIDTH) return;
    x = (uint16_t)((BOOT_LCD_WIDTH - length * character_width) / 2U);
    while(*text != '\0') {
        DrawCharacter(x, y, *text, scale, foreground, background);
        x = (uint16_t)(x + character_width);
        ++text;
    }
}

static void DrawProgressShell(uint16_t y)
{
    DrawFrame(19U, y, 202U, 12U, 2U, BOOT_COLOR_GRAY);
    FillRect(21U, (uint16_t)(y + 2U), 198U, 8U, BOOT_COLOR_DARK_GRAY);
}

void BootDisplay_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    static const uint8_t porch[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
    static const uint8_t power[] = {0xA4U, 0xA1U};
    static const uint8_t gamma_positive[] = {
        0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
        0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U
    };
    static const uint8_t gamma_negative[] = {
        0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
        0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U
    };
    uint8_t value;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(GPIOB, LCD_RST_PIN | LCD_CS_PIN | LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, LCD_BL_PIN, GPIO_PIN_RESET);
    gpio.Pin = LCD_RST_PIN | LCD_CS_PIN | LCD_DC_PIN | LCD_BL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    __HAL_RCC_SPI1_FORCE_RESET();
    __HAL_RCC_SPI1_RELEASE_RESET();
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_CPOL | SPI_CR1_CPHA |
                SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR2 = 0U;
    SPI1->CR1 |= SPI_CR1_SPE;

    LcdSelect(1U);
    HAL_GPIO_WritePin(GPIOB, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(80U);
    HAL_GPIO_WritePin(GPIOB, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(100U);
    LcdCommand(0x11U);
    HAL_Delay(120U);
    value = 0x00U; LcdCommandData(0x36U, &value, 1U);
    value = 0x05U; LcdCommandData(0x3AU, &value, 1U);
    LcdCommandData(0xB2U, porch, sizeof(porch));
    value = 0x35U; LcdCommandData(0xB7U, &value, 1U);
    value = 0x19U; LcdCommandData(0xBBU, &value, 1U);
    value = 0x2CU; LcdCommandData(0xC0U, &value, 1U);
    value = 0x01U; LcdCommandData(0xC2U, &value, 1U);
    value = 0x12U; LcdCommandData(0xC3U, &value, 1U);
    value = 0x20U; LcdCommandData(0xC4U, &value, 1U);
    value = 0x0FU; LcdCommandData(0xC6U, &value, 1U);
    LcdCommandData(0xD0U, power, sizeof(power));
    LcdCommandData(0xE0U, gamma_positive, sizeof(gamma_positive));
    LcdCommandData(0xE1U, gamma_negative, sizeof(gamma_negative));
    LcdCommand(0x21U);
    LcdCommand(0x29U);
    HAL_Delay(20U);
    FillRect(0U, 0U, BOOT_LCD_WIDTH, BOOT_LCD_HEIGHT, BOOT_COLOR_BLACK);
}

void BootDisplay_ShowStartup(void)
{
    FillRect(0U, 0U, BOOT_LCD_WIDTH, BOOT_LCD_HEIGHT, BOOT_COLOR_BLACK);
    DrawFrame(94U, 24U, 52U, 34U, 3U, BOOT_COLOR_CYAN);
    FillRect(111U, 17U, 18U, 7U, BOOT_COLOR_CYAN);
    FillRect(111U, 58U, 18U, 7U, BOOT_COLOR_CYAN);
    DrawTextCentered(76U, "OV-WATCH", 3U, BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
    DrawTextCentered(112U, "SECURE BOOT", 2U, BOOT_COLOR_CYAN, BOOT_COLOR_BLACK);
    DrawTextCentered(166U, "DOUBLE-CLICK KEY1", 1U, BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
    DrawTextCentered(181U, "FOR OTA UPDATE", 1U, BOOT_COLOR_GRAY, BOOT_COLOR_BLACK);
    DrawProgressShell(218U);
    DrawTextCentered(244U, "VERIFYING FIRMWARE", 1U, BOOT_COLOR_GRAY, BOOT_COLOR_BLACK);
    s_startup_progress_width = 0U;
    HAL_GPIO_WritePin(GPIOB, LCD_BL_PIN, GPIO_PIN_SET);
}

void BootDisplay_SetStartupProgress(uint32_t elapsed_ms, uint32_t total_ms)
{
    uint16_t width;

    if(total_ms == 0U) return;
    if(elapsed_ms > total_ms) elapsed_ms = total_ms;
    width = (uint16_t)(198UL * elapsed_ms / total_ms);
    if(width > s_startup_progress_width) {
        FillRect((uint16_t)(21U + s_startup_progress_width), 220U,
                 (uint16_t)(width - s_startup_progress_width), 8U, BOOT_COLOR_BLUE);
        s_startup_progress_width = width;
    }
}

void BootDisplay_ShowRecovery(const char *reason)
{
    FillRect(0U, 0U, BOOT_LCD_WIDTH, BOOT_LCD_HEIGHT, BOOT_COLOR_BLACK);
    DrawFrame(83U, 32U, 74U, 58U, 4U, BOOT_COLOR_RED);
    DrawTextCentered(48U, "OTA", 3U, BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
    DrawTextCentered(112U, "RECOVERY MODE", 2U, BOOT_COLOR_RED, BOOT_COLOR_BLACK);
    DrawTextCentered(154U, (reason != NULL) ? reason : "WAITING", 1U,
                     BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
    DrawTextCentered(178U, "SEND SIGNED PACKAGE", 1U, BOOT_COLOR_GRAY, BOOT_COLOR_BLACK);
    DrawTextCentered(193U, "UART1 9600 8N1", 1U, BOOT_COLOR_GRAY, BOOT_COLOR_BLACK);
    DrawProgressShell(225U);
    s_ota_progress_width = 0U;
}

void BootDisplay_ShowOtaReceiving(void)
{
    DrawTextCentered(154U, "RECEIVING UPDATE", 1U,
                     BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
}

void BootDisplay_SetOtaProgress(uint32_t received, uint32_t total)
{
    uint16_t width;

    if(total == 0U) return;
    if(received > total) received = total;
    width = (uint16_t)(198UL * received / total);
    if(width > s_ota_progress_width) {
        FillRect((uint16_t)(21U + s_ota_progress_width), 227U,
                 (uint16_t)(width - s_ota_progress_width), 8U, BOOT_COLOR_GREEN);
        s_ota_progress_width = width;
    }
}

void BootDisplay_ShowUpdateComplete(void)
{
    FillRect(0U, 0U, BOOT_LCD_WIDTH, BOOT_LCD_HEIGHT, BOOT_COLOR_BLACK);
    DrawTextCentered(91U, "UPDATE", 3U, BOOT_COLOR_WHITE, BOOT_COLOR_BLACK);
    DrawTextCentered(130U, "COMPLETE", 3U, BOOT_COLOR_GREEN, BOOT_COLOR_BLACK);
    DrawTextCentered(180U, "RESTARTING...", 1U, BOOT_COLOR_GRAY, BOOT_COLOR_BLACK);
}

void BootDisplay_ShowVerified(void)
{
    BootDisplay_SetStartupProgress(1U, 1U);
    FillRect(0U, 244U, BOOT_LCD_WIDTH, 12U, BOOT_COLOR_BLACK);
    DrawTextCentered(244U, "VERIFIED - STARTING", 1U,
                     BOOT_COLOR_GREEN, BOOT_COLOR_BLACK);
}

void BootDisplay_Deinit(void)
{
    LcdSelect(0U);
    SPI1->CR1 &= ~SPI_CR1_SPE;
    __HAL_RCC_SPI1_FORCE_RESET();
    __HAL_RCC_SPI1_RELEASE_RESET();
    HAL_GPIO_WritePin(GPIOB, LCD_BL_PIN, GPIO_PIN_RESET);
}
