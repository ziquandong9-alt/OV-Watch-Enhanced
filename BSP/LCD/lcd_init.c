#include "lcd_init.h"
#include "lcd.h"

static void LCD_WriteBufferBlocking(const uint8_t *buffer, uint16_t length)
{
    /* 初始化命令量小，阻塞 SPI 能简化上电时序。 */
    LCD_WaitForDMA();

    if (LL_SPI_IsEnabled(SPI1) == 0U)
    {
        LL_SPI_Enable(SPI1);
    }

    LL_SPI_DisableDMAReq_TX(SPI1);

    while (length != 0U)
    {
        while (LL_SPI_IsActiveFlag_TXE(SPI1) == 0U)
        {
        }

        LL_SPI_TransmitData8(SPI1, *buffer);
        ++buffer;
        --length;
    }

    while (LL_SPI_IsActiveFlag_TXE(SPI1) == 0U)
    {
    }

    while (LL_SPI_IsActiveFlag_BSY(SPI1) != 0U)
    {
    }

    /* FULL_DUPLEX 时清除无用接收数据造成的 OVR；HALF_DUPLEX_TX 下通常不会置位。 */
    if (LL_SPI_IsActiveFlag_OVR(SPI1) != 0U)
    {
        LL_SPI_ClearFlag_OVR(SPI1);
    }
}

void LCD_GPIO_Init(void)
{
    /* 配置 RES/DC/CS/BLK 控制脚并设置安全初始电平。 */
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = LCD_RES_PIN | LCD_CS_PIN | LCD_DC_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    LCD_RES_HIGH();
    LCD_CS_HIGH();
    LCD_DC_HIGH();
}

void LCD_Writ_Bus(uint8_t data)
{
    /* 最底层单字节 SPI 写，发送前确保无异步 DMA 占用。 */
    LCD_WriteBufferBlocking(&data, 1U);
}

void LCD_WR_REG(uint8_t command)
{
    /* DC=0 表示 ST7789 命令。 */
    LCD_WaitForDMA();
    LCD_DC_LOW();
    LCD_WriteBufferBlocking(&command, 1U);
    LCD_DC_HIGH();
}

void LCD_WR_DATA8(uint8_t data)
{
    /* DC=1 表示命令参数或像素数据。 */
    LCD_DC_HIGH();
    LCD_WriteBufferBlocking(&data, 1U);
}

void LCD_WR_DATA(uint16_t data)
{
    /* 16 位参数按高字节在前发送。 */
    uint8_t bytes[2];

    bytes[0] = (uint8_t)(data >> 8);
    bytes[1] = (uint8_t)data;

    LCD_DC_HIGH();
    LCD_WriteBufferBlocking(bytes, 2U);
}

void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* 写 CASET/RASET/RAMWR，限定下一批像素的矩形窗口。 */
    LCD_WR_REG(0x2AU);
    LCD_WR_DATA(x1);
    LCD_WR_DATA(x2);

    LCD_WR_REG(0x2BU);
    LCD_WR_DATA(y1);
    LCD_WR_DATA(y2);

    LCD_WR_REG(0x2CU);
}

void LCD_Set_Light(uint8_t percent)
{
    /* 把 0~100% 转换为 TIM3 PWM 比较值并夹紧输入。 */
    if (percent > 100U)
    {
        percent = 100U;
    }

    __HAL_TIM_SET_COMPARE(&htim3,
                          TIM_CHANNEL_3,
                          ((uint32_t)__HAL_TIM_GET_AUTORELOAD(&htim3) + 1U) * percent / 100U);
}

void LCD_Open_Light(void)
{
    /* 启动背光 PWM，并恢复默认/最近设置的占空比。 */
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

void LCD_Close_Light(void)
{
    /* 将背光占空比降为 0；LCD 控制器本身仍可能工作。 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0U);
}

void LCD_ST7789_SleepIn(void)
{
    /* 等 DMA 后发送 Sleep In，进入 STOP 前必须完成。 */
    LCD_WR_REG(0x10U);
    HAL_Delay(120U);
}

void LCD_ST7789_SleepOut(void)
{
    /* Sleep Out 后等待面板内部电源稳定再刷新。 */
    LCD_WR_REG(0x11U);
    HAL_Delay(120U);
}

void LCD_Init(void)
{
    /* 硬复位并配置像素格式、方向、Gamma 和显示开关。 */
    static const uint8_t gamma_positive[] = {
        0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
        0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U
    };
    static const uint8_t gamma_negative[] = {
        0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
        0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U
    };

    if (LL_SPI_IsEnabled(SPI1) == 0U)
    {
        LL_SPI_Enable(SPI1);
    }

    LCD_GPIO_Init();
    LCD_CS_LOW();

    LCD_RES_LOW();
    HAL_Delay(100U);
    LCD_RES_HIGH();
    HAL_Delay(100U);

    LCD_WR_REG(0x11U);
    HAL_Delay(120U);

    LCD_WR_REG(0x36U);
#if USE_HORIZONTAL == 0U
    LCD_WR_DATA8(0x00U);
#elif USE_HORIZONTAL == 1U
    LCD_WR_DATA8(0xC0U);
#elif USE_HORIZONTAL == 2U
    LCD_WR_DATA8(0x70U);
#else
    LCD_WR_DATA8(0xA0U);
#endif

    LCD_WR_REG(0x3AU);
    LCD_WR_DATA8(0x05U);

    LCD_WR_REG(0xB2U);
    LCD_WR_DATA8(0x0CU);
    LCD_WR_DATA8(0x0CU);
    LCD_WR_DATA8(0x00U);
    LCD_WR_DATA8(0x33U);
    LCD_WR_DATA8(0x33U);

    LCD_WR_REG(0xB7U);
    LCD_WR_DATA8(0x35U);

    LCD_WR_REG(0xBBU);
    LCD_WR_DATA8(0x19U);

    LCD_WR_REG(0xC0U);
    LCD_WR_DATA8(0x2CU);

    LCD_WR_REG(0xC2U);
    LCD_WR_DATA8(0x01U);

    LCD_WR_REG(0xC3U);
    LCD_WR_DATA8(0x12U);

    LCD_WR_REG(0xC4U);
    LCD_WR_DATA8(0x20U);

    LCD_WR_REG(0xC6U);
    LCD_WR_DATA8(0x0FU);

    LCD_WR_REG(0xD0U);
    LCD_WR_DATA8(0xA4U);
    LCD_WR_DATA8(0xA1U);

    LCD_WR_REG(0xE0U);
    LCD_WriteBufferBlocking(gamma_positive, (uint16_t)sizeof(gamma_positive));

    LCD_WR_REG(0xE1U);
    LCD_WriteBufferBlocking(gamma_negative, (uint16_t)sizeof(gamma_negative));

    LCD_WR_REG(0x21U);
    LCD_WR_REG(0x29U);
    HAL_Delay(20U);

    LCD_Open_Light();
    LCD_Set_Light(LCD_DEFAULT_BRIGHTNESS);
}
