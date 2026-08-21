#include "lcd.h"

/* ST7789 像素层：LVGL 使用异步 DMA 刷块，其余点线文字函数供裸机测试。 */
#include "lcd_init.h"
#include "font8x8_basic.h"
#include "spi.h"

#define LCD_DMA_MAX_CHUNK_BYTES 65534UL
#define LCD_DMA                  DMA2
#define LCD_DMA_STREAM           LL_DMA_STREAM_2

typedef enum
{
    LCD_DMA_IDLE = 0,
    LCD_DMA_ACTIVE,
    LCD_DMA_WAIT_SPI_IDLE,
    LCD_DMA_ERROR
} LCD_DMA_State_t;

static volatile LCD_DMA_State_t s_dma_state = LCD_DMA_IDLE;
static const uint8_t * volatile s_next_dma_address = NULL;
static volatile uint32_t s_remaining_dma_bytes = 0U;
static volatile uint8_t s_notify_flush = 0U;
static LCD_CallbackFunc_t s_flush_complete_callback = NULL;

static HAL_StatusTypeDef LCD_StartNextDMAChunk(void)
{
    /* DMA 的 16 位 NDTR 放不下超大传输，因此把像素流拆成连续 chunk。 */
    uint32_t chunk_bytes;

    if ((s_next_dma_address == NULL) || (s_remaining_dma_bytes == 0U))
    {
        return HAL_ERROR;
    }

    chunk_bytes = s_remaining_dma_bytes;
    if (chunk_bytes > LCD_DMA_MAX_CHUNK_BYTES)
    {
        chunk_bytes = LCD_DMA_MAX_CHUNK_BYTES;
    }

    if (LL_DMA_IsEnabledStream(LCD_DMA, LCD_DMA_STREAM) != 0U)
    {
        return HAL_BUSY;
    }

    if (LL_SPI_IsEnabled(SPI1) == 0U)
    {
        LL_SPI_Enable(SPI1);
    }

    LL_SPI_DisableDMAReq_TX(SPI1);

    LL_DMA_ClearFlag_TC2(LCD_DMA);
    LL_DMA_ClearFlag_HT2(LCD_DMA);
    LL_DMA_ClearFlag_TE2(LCD_DMA);
    LL_DMA_ClearFlag_DME2(LCD_DMA);
    LL_DMA_ClearFlag_FE2(LCD_DMA);

    LL_DMA_SetPeriphAddress(LCD_DMA,
                            LCD_DMA_STREAM,
                            (uint32_t)&SPI1->DR);
    LL_DMA_SetMemoryAddress(LCD_DMA,
                            LCD_DMA_STREAM,
                            (uint32_t)s_next_dma_address);
    LL_DMA_SetDataLength(LCD_DMA,
                         LCD_DMA_STREAM,
                         chunk_bytes);

    /* 在使能 DMA 前更新链表状态，避免极短传输的完成中断看到旧值。 */
    s_next_dma_address += chunk_bytes;
    s_remaining_dma_bytes -= chunk_bytes;
    s_dma_state = LCD_DMA_ACTIVE;

    LL_DMA_EnableIT_TC(LCD_DMA, LCD_DMA_STREAM);
    LL_DMA_EnableIT_TE(LCD_DMA, LCD_DMA_STREAM);

    LCD_DC_HIGH();
    __DMB();
    LL_DMA_EnableStream(LCD_DMA, LCD_DMA_STREAM);
    LL_SPI_EnableDMAReq_TX(SPI1);

    return HAL_OK;
}

static HAL_StatusTypeDef LCD_StartDMAChain(const uint8_t *buffer,
                                           uint32_t byte_count,
                                           uint8_t notify_flush)
{
    /* 建立整条传输的地址/剩余长度状态，再启动第一个 chunk。 */
    if ((buffer == NULL) || (byte_count == 0U))
    {
        return HAL_ERROR;
    }

    /* 异步入口不在这里阻塞：调用方可选择等待，或在忙时稍后重试。 */
    LCD_Service();
    if (s_dma_state != LCD_DMA_IDLE)
    {
        return HAL_BUSY;
    }

    s_next_dma_address = buffer;
    s_remaining_dma_bytes = byte_count;
    s_notify_flush = notify_flush;

    return LCD_StartNextDMAChunk();
}

void LCD_Service(void)
{
    /* 主循环收尾 DMA 并执行完成回调；不在中断里直接调用 LVGL。 */
    LCD_CallbackFunc_t callback;

    if (s_dma_state == LCD_DMA_WAIT_SPI_IDLE)
    {
        /* 最后一包后只在任务/主循环检查 SPI 物理发送完成。 */
        if (LL_SPI_IsActiveFlag_BSY(SPI1) == 0U)
        {
            if (LL_SPI_IsActiveFlag_OVR(SPI1) != 0U)
            {
                LL_SPI_ClearFlag_OVR(SPI1);
            }

            s_dma_state = LCD_DMA_IDLE;
            callback = s_flush_complete_callback;

            if ((s_notify_flush != 0U) && (callback != NULL))
            {
                s_notify_flush = 0U;
                callback();
            }
            else
            {
                s_notify_flush = 0U;
            }
        }
    }
    else if (s_dma_state == LCD_DMA_ERROR)
    {
        callback = s_flush_complete_callback;
        LL_SPI_DisableDMAReq_TX(SPI1);
        LL_DMA_DisableStream(LCD_DMA, LCD_DMA_STREAM);
        s_next_dma_address = NULL;
        s_remaining_dma_bytes = 0U;
        s_dma_state = LCD_DMA_IDLE;
        if ((s_notify_flush != 0U) && (callback != NULL))
        {
            s_notify_flush = 0U;
            callback();
        }
        else
        {
            s_notify_flush = 0U;
        }
    }
}

void LCD_WaitForDMA(void)
{
    /* 发 LCD 命令或关闭 SPI 前必须等像素 DMA 完全结束。 */
    while (s_dma_state != LCD_DMA_IDLE)
    {
        LCD_Service();
    }
}

uint8_t LCD_IsDMABusy(void)
{
    /* 对外只暴露忙/闲，不泄露内部 DMA 阶段。 */
    LCD_Service();
    return (s_dma_state != LCD_DMA_IDLE) ? 1U : 0U;
}

void LCD_Set_Flush_Complete_Callback(LCD_CallbackFunc_t callback)
{
    /* LVGL port 注册完成通知，DMA 链结束后归还 draw buffer。 */
    s_flush_complete_callback = callback;
}

HAL_StatusTypeDef LCD_Color_Fill_DMA(uint16_t x1,
                                     uint16_t y1,
                                     uint16_t x2,
                                     uint16_t y2,
                                     const uint16_t *pixels)
{
    /* 设置地址窗口后，把 RGB565 缓冲区异步发送到矩形区域。 */
    uint32_t width;
    uint32_t height;
    uint32_t byte_count;

    if ((pixels == NULL) || (x2 < x1) || (y2 < y1))
    {
        return HAL_ERROR;
    }

    width = (uint32_t)x2 - x1 + 1U;
    height = (uint32_t)y2 - y1 + 1U;
    byte_count = width * height * 2U;

    /* 真正的异步语义：上一帧未结束时立即返回，不占住当前任务。 */
    LCD_Service();
    if (s_dma_state != LCD_DMA_IDLE)
    {
        return HAL_BUSY;
    }

    LCD_Address_Set(x1,
                    (uint16_t)(y1 + LCD_Y_OFFSET),
                    x2,
                    (uint16_t)(y2 + LCD_Y_OFFSET));

    return LCD_StartDMAChain((const uint8_t *)pixels,
                             byte_count,
                             1U);
}

void LCD_Color_Fill(uint16_t x1,
                    uint16_t y1,
                    uint16_t x2,
                    uint16_t y2,
                    uint16_t *pixels)
{
    /* 阻塞版本用于初始化/测试；正常界面刷新优先使用 DMA。 */
    (void)LCD_Color_Fill_DMA(x1, y1, x2, y2, pixels);
}

void LCD_Fill(uint16_t x1,
              uint16_t y1,
              uint16_t x2,
              uint16_t y2,
              uint16_t color)
{
    /* 用固定行块重复填纯色，避免分配一份全屏临时缓冲。 */
    /* uint32_t 数组同时保证 4 字节对齐；每次写入两个相同的 RGB565 像素。 */
    static uint32_t chunk_buffer_words[
        (LCD_W * LCD_FILL_CHUNK_LINES + 1U) / 2U
    ];
    uint8_t *chunk_buffer = (uint8_t *)chunk_buffer_words;
    uint16_t width;
    uint16_t remaining_lines;
    uint16_t current_lines;
    uint16_t wire_color;
    uint32_t packed_color;
    uint32_t pixel_count;
    uint32_t word_count;
    uint32_t word;
    uint32_t chunk_bytes;

    if ((x2 <= x1) || (y2 <= y1))
    {
        return;
    }

    width = x2 - x1;
    if (width > LCD_W)
    {
        return;
    }

    /*
     * Cortex-M4 为小端：先交换 RGB565 的两个字节，落入内存后即为
     * LCD 所需的 [高字节, 低字节, 高字节, 低字节] 顺序。
     */
    wire_color = (uint16_t)((color >> 8) | (color << 8));
    packed_color = (uint32_t)wire_color |
                   ((uint32_t)wire_color << 16);
    pixel_count = (uint32_t)width * LCD_FILL_CHUNK_LINES;
    word_count = pixel_count / 2U;

    for (word = 0U; word < word_count; ++word)
    {
        chunk_buffer_words[word] = packed_color;
    }

    if ((pixel_count & 1U) != 0U)
    {
        ((uint16_t *)chunk_buffer)[pixel_count - 1U] = wire_color;
    }

    LCD_WaitForDMA();
    LCD_Address_Set(x1,
                    (uint16_t)(y1 + LCD_Y_OFFSET),
                    (uint16_t)(x2 - 1U),
                    (uint16_t)(y2 - 1U + LCD_Y_OFFSET));

    remaining_lines = y2 - y1;
    while (remaining_lines != 0U)
    {
        current_lines = remaining_lines;
        if (current_lines > LCD_FILL_CHUNK_LINES)
        {
            current_lines = LCD_FILL_CHUNK_LINES;
        }

        chunk_bytes = (uint32_t)width * current_lines * 2U;

        if (LCD_StartDMAChain(chunk_buffer,
                              chunk_bytes,
                              0U) != HAL_OK)
        {
            break;
        }

        LCD_WaitForDMA();
        remaining_lines = (uint16_t)(remaining_lines - current_lines);
    }
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    /* 单点会产生一次地址窗口命令，批量绘制时效率很低。 */
    LCD_WaitForDMA();
    LCD_Address_Set(x,
                    (uint16_t)(y + LCD_Y_OFFSET),
                    x,
                    (uint16_t)(y + LCD_Y_OFFSET));
    LCD_WR_DATA(color);
}

void LCD_DrawLine(uint16_t x1,
                  uint16_t y1,
                  uint16_t x2,
                  uint16_t y2,
                  uint16_t color)
{
    /* 整数 Bresenham 算法连接两点，不使用浮点。 */
    int32_t dx = (x2 >= x1) ? (int32_t)(x2 - x1) : (int32_t)(x1 - x2);
    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t dy = (y2 >= y1) ? -(int32_t)(y2 - y1) : -(int32_t)(y1 - y2);
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t error = dx + dy;
    int32_t twice_error;

    for (;;)
    {
        LCD_DrawPoint(x1, y1, color);

        if ((x1 == x2) && (y1 == y2))
        {
            break;
        }

        twice_error = 2 * error;
        if (twice_error >= dy)
        {
            error += dy;
            x1 = (uint16_t)((int32_t)x1 + sx);
        }
        if (twice_error <= dx)
        {
            error += dx;
            y1 = (uint16_t)((int32_t)y1 + sy);
        }
    }
}

void LCD_DrawRectangle(uint16_t x1,
                       uint16_t y1,
                       uint16_t x2,
                       uint16_t y2,
                       uint16_t color)
{
    /* 矩形边框由四条直线组成，不填内部。 */
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
}

void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t radius, uint16_t color)
{
    /* 中点圆算法利用八向对称，只计算一个八分圆。 */
    int32_t a = 0;
    int32_t b = radius;

    while (a <= b)
    {
        LCD_DrawPoint((uint16_t)(x0 - b), (uint16_t)(y0 - a), color);
        LCD_DrawPoint((uint16_t)(x0 + b), (uint16_t)(y0 - a), color);
        LCD_DrawPoint((uint16_t)(x0 - a), (uint16_t)(y0 + b), color);
        LCD_DrawPoint((uint16_t)(x0 - a), (uint16_t)(y0 - b), color);
        LCD_DrawPoint((uint16_t)(x0 + b), (uint16_t)(y0 + a), color);
        LCD_DrawPoint((uint16_t)(x0 + a), (uint16_t)(y0 - b), color);
        LCD_DrawPoint((uint16_t)(x0 + a), (uint16_t)(y0 + b), color);
        LCD_DrawPoint((uint16_t)(x0 - b), (uint16_t)(y0 + a), color);

        ++a;
        if ((a * a + b * b) > ((int32_t)radius * radius))
        {
            --b;
        }
    }
}

void LCD_ShowChinese(uint16_t x, uint16_t y, uint8_t *text,
                     uint16_t foreground, uint16_t background,
                     uint8_t size, uint8_t overlay)
{
    /* 从中文字模表查编码并按指定字号逐像素绘制。 */
    while (*text != 0U)
    {
        if (size == 12U)
            LCD_ShowChinese12x12(x, y, text, foreground, background, size, overlay);
        else if (size == 16U)
            LCD_ShowChinese16x16(x, y, text, foreground, background, size, overlay);
        else if (size == 24U)
            LCD_ShowChinese24x24(x, y, text, foreground, background, size, overlay);
        else if (size == 32U)
            LCD_ShowChinese32x32(x, y, text, foreground, background, size, overlay);
        else
            return;

        text += 2;
        x = (uint16_t)(x + size);
    }
}

/* 通用渲染器：读取字模位，为 1 画前景，否则按 overlay 处理背景。 */
static void LCD_ShowChineseGlyph(uint16_t x, uint16_t y,
                                 const uint8_t *mask, uint16_t mask_size,
                                 uint8_t glyph_size,
                                 uint16_t foreground, uint16_t background,
                                 uint8_t overlay)
{
    uint16_t byte_index;
    uint8_t bit_index;
    uint16_t pixel_count = 0U;
    uint16_t x_origin = x;

    if (overlay == 0U)
    {
        LCD_Address_Set(x, y,
                        (uint16_t)(x + glyph_size - 1U),
                        (uint16_t)(y + glyph_size - 1U));
    }

    for (byte_index = 0U; byte_index < mask_size; ++byte_index)
    {
        for (bit_index = 0U; bit_index < 8U; ++bit_index)
        {
            if (overlay == 0U)
            {
                LCD_WR_DATA((mask[byte_index] & (1U << bit_index)) != 0U
                                ? foreground : background);
                ++pixel_count;
                if ((pixel_count % glyph_size) == 0U)
                    break;
            }
            else
            {
                if ((mask[byte_index] & (1U << bit_index)) != 0U)
                    LCD_DrawPoint(x, y, foreground);

                ++x;
                if ((x - x_origin) == glyph_size)
                {
                    x = x_origin;
                    ++y;
                    break;
                }
            }
        }
    }
}

/* 在 12×12 字模表查找并交给通用渲染器。 */
void LCD_ShowChinese12x12(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay)
{
    uint16_t index;
    (void)size;

    for (index = 0U; index < (uint16_t)(sizeof(tfont12) / sizeof(typFNT_GB12)); ++index)
    {
        if ((tfont12[index].Index[0] == text[0]) &&
            (tfont12[index].Index[1] == text[1]))
        {
            LCD_ShowChineseGlyph(x, y, tfont12[index].Msk,
                                 (uint16_t)sizeof(tfont12[index].Msk), 12U,
                                 foreground, background, overlay);
            return;
        }
    }
}

/* 在 16×16 字模表中查找两个字节编码。 */
void LCD_ShowChinese16x16(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay)
{
    uint16_t index;
    (void)size;

    for (index = 0U; index < (uint16_t)(sizeof(tfont16) / sizeof(typFNT_GB16)); ++index)
    {
        if ((tfont16[index].Index[0] == text[0]) &&
            (tfont16[index].Index[1] == text[1]))
        {
            LCD_ShowChineseGlyph(x, y, tfont16[index].Msk,
                                 (uint16_t)sizeof(tfont16[index].Msk), 16U,
                                 foreground, background, overlay);
            return;
        }
    }
}

/* 24×24 字模清晰，但逐点透明绘制的 SPI 开销更高。 */
void LCD_ShowChinese24x24(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay)
{
    uint16_t index;
    (void)size;

    for (index = 0U; index < (uint16_t)(sizeof(tfont24) / sizeof(typFNT_GB24)); ++index)
    {
        if ((tfont24[index].Index[0] == text[0]) &&
            (tfont24[index].Index[1] == text[1]))
        {
            LCD_ShowChineseGlyph(x, y, tfont24[index].Msk,
                                 (uint16_t)sizeof(tfont24[index].Msk), 24U,
                                 foreground, background, overlay);
            return;
        }
    }
}

/* 32×32 适合大标题；正常 LVGL 页面优先使用字体引擎。 */
void LCD_ShowChinese32x32(uint16_t x, uint16_t y, uint8_t *text,
                         uint16_t foreground, uint16_t background,
                         uint8_t size, uint8_t overlay)
{
    uint16_t index;
    (void)size;

    for (index = 0U; index < (uint16_t)(sizeof(tfont32) / sizeof(typFNT_GB32)); ++index)
    {
        if ((tfont32[index].Index[0] == text[0]) &&
            (tfont32[index].Index[1] == text[1]))
        {
            LCD_ShowChineseGlyph(x, y, tfont32[index].Msk,
                                 (uint16_t)sizeof(tfont32[index].Msk), 32U,
                                 foreground, background, overlay);
            return;
        }
    }
}

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t character,
                  uint16_t foreground, uint16_t background,
                  uint8_t size, uint8_t overlay)
{
    /* 将公共领域 8×8 点阵按目标字号缩放，供裸机测试文字使用。 */
    uint8_t width;
    uint8_t target_x;
    uint8_t target_y;
    uint8_t source_x;
    uint8_t source_y;
    uint8_t source_row;
    uint8_t pixel_on;

    if ((character < (uint8_t)' ') || (character > (uint8_t)'~'))
        return;
    if ((size != 12U) && (size != 16U) &&
        (size != 24U) && (size != 32U))
        return;

    width = size / 2U;
    if (overlay == 0U)
        LCD_Address_Set(x, y, (uint16_t)(x + width - 1U),
                        (uint16_t)(y + size - 1U));

    for (target_y = 0U; target_y < size; ++target_y)
    {
        source_y = (uint8_t)(((uint16_t)target_y * 8U) / size);
        source_row = lcd_font8x8_basic[character][source_y];

        for (target_x = 0U; target_x < width; ++target_x)
        {
            source_x = (uint8_t)(((uint16_t)target_x * 8U) / width);
            pixel_on = ((source_row & (uint8_t)(1U << source_x)) != 0U)
                           ? 1U : 0U;

            if (overlay == 0U)
            {
                LCD_WR_DATA((pixel_on != 0U) ? foreground : background);
            }
            else if (pixel_on != 0U)
            {
                LCD_DrawPoint((uint16_t)(x + target_x),
                              (uint16_t)(y + target_y),
                              foreground);
            }
        }
    }
}

void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *text,
                    uint16_t foreground, uint16_t background,
                    uint8_t size, uint8_t overlay)
{
    /* 逐字符推进坐标，复用 LCD_ShowChar。 */
    if (text == NULL)
        return;

    while (*text != 0U)
    {
        LCD_ShowChar(x, y, *text, foreground, background, size, overlay);
        x = (uint16_t)(x + size / 2U);
        ++text;
    }
}

uint32_t mypow(uint8_t base, uint8_t exponent)
{
    /* 小整数幂供数字逐位提取，避免引入浮点 pow。 */
    uint32_t result = 1U;
    while (exponent-- != 0U)
        result *= base;
    return result;
}

void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t number,
                    uint8_t length, uint16_t foreground,
                    uint16_t background, uint8_t size)
{
    /* 从高位到低位显示定长整数并处理前导零。 */
    uint8_t position;
    uint8_t digit;
    uint8_t started = 0U;
    uint8_t width = size / 2U;

    for (position = 0U; position < length; ++position)
    {
        digit = (uint8_t)((number / mypow(10U, (uint8_t)(length - position - 1U))) % 10U);
        if ((started == 0U) && (position < (uint8_t)(length - 1U)) && (digit == 0U))
        {
            LCD_ShowChar((uint16_t)(x + position * width), y, ' ',
                         foreground, background, size, 0U);
            continue;
        }

        started = 1U;
        LCD_ShowChar((uint16_t)(x + position * width), y,
                     (uint8_t)(digit + '0'), foreground, background, size, 0U);
    }
}

void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float number,
                       uint8_t length, uint16_t foreground,
                       uint16_t background, uint8_t size)
{
    /* 按固定小数位放大后复用整数数字绘制逻辑。 */
    uint8_t position;
    uint8_t digit;
    uint8_t width = size / 2U;
    uint32_t scaled = (uint32_t)(number * 100.0f);

    for (position = 0U; position < length; ++position)
    {
        if (position == (uint8_t)(length - 2U))
        {
            LCD_ShowChar((uint16_t)(x + position * width), y, '.',
                         foreground, background, size, 0U);
            continue;
        }

        digit = (uint8_t)((scaled / mypow(10U,
                   (uint8_t)(length - position - (position < length - 2U ? 2U : 1U)))) % 10U);
        LCD_ShowChar((uint16_t)(x + position * width), y,
                     (uint8_t)(digit + '0'), foreground, background, size, 0U);
    }
}

void LCD_ShowPicture(uint16_t x, uint16_t y,
                     uint16_t length, uint16_t width,
                     const uint8_t picture[])
{
    /* 图片必须是与 LCD 字节序一致的连续 RGB565 数据。 */
    uint32_t pixel;
    uint32_t pixel_count;

    if ((picture == NULL) || (length == 0U) || (width == 0U))
        return;

    LCD_Address_Set(x, y,
                    (uint16_t)(x + length - 1U),
                    (uint16_t)(y + width - 1U));

    pixel_count = (uint32_t)length * width;
    for (pixel = 0U; pixel < pixel_count; ++pixel)
    {
        LCD_WR_DATA8(picture[pixel * 2U]);
        LCD_WR_DATA8(picture[pixel * 2U + 1U]);
    }
}

void LCD_DMA_TX_IRQHandler(void)
{
    /* 中断只清标志并推进 chunk，LVGL 完成回调留给主循环。 */
    if (LL_DMA_IsActiveFlag_TE2(LCD_DMA) != 0U)
    {
        LL_DMA_ClearFlag_TE2(LCD_DMA);
        LL_SPI_DisableDMAReq_TX(SPI1);
        LL_DMA_DisableStream(LCD_DMA, LCD_DMA_STREAM);
        s_dma_state = LCD_DMA_ERROR;
        return;
    }

    if (LL_DMA_IsActiveFlag_TC2(LCD_DMA) != 0U)
    {
        LL_DMA_ClearFlag_TC2(LCD_DMA);

        if (s_remaining_dma_bytes != 0U)
        {
            /* LL 续包只有有限次寄存器写入；不在 ISR 中等待 BSY。 */
            if (LCD_StartNextDMAChunk() != HAL_OK)
            {
                s_dma_state = LCD_DMA_ERROR;
            }
        }
        else
        {
            LL_SPI_DisableDMAReq_TX(SPI1);
            s_dma_state = LCD_DMA_WAIT_SPI_IDLE;
        }
    }
}
