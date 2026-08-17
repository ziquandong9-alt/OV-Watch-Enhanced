#include "power_manager.h"

#include "CST816.h"
#include "dma.h"
#include "lcd.h"
#include "lcd_init.h"
#include "main.h"
#include "rtc.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "usart.h"
#include "ble_manager.h"
#include "adc.h"

static volatile uint8_t s_wake_flags;
static uint32_t s_rtc_wake_elapsed_ms = 1000U;
static uint8_t s_peripheral_suspend_level;

/* RTC 唤醒时钟为 32768 Hz/16：2048 约 1 秒，256 约 125 ms。 */
#define RTC_NORMAL_WAKE_COUNTER       2048U
#define RTC_TOUCH_POLL_COUNTER        256U
#define RTC_TOUCH_POLL_ELAPSED_MS     125U

extern void SystemClock_Config(void);

void PowerManager_Init(void)
{
    /* 配置 KEY1 为下降沿 EXTI，使 MCU 在 STOP 中也能被物理按键唤醒。 */
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = KEY1_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY1_GPIO_Port, &gpio);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* 已公开核心板没有把 TP_INT 接到 MCU，因此触摸只能靠 RTC 周期轮询。 */
    CST816_IIC_WriteREG(DisAutoSleep, 1U);
}

void PowerManager_BeginStopSession(void)
{
    /* 清掉旧手势，并改成每 125 ms RTC 短暂唤醒后轮询触摸控制器。 */
    CST816_RESET();
    CST816_IIC_WriteREG(DisAutoSleep, 1U);
    CST816_Config_MotionMask(M_ALLENABLE);
    (void)CST816_IIC_ReadREG(GestureID);

    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    if(HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_TOUCH_POLL_COUNTER,
                                  RTC_WAKEUPCLOCK_RTCCLK_DIV16) == HAL_OK) {
        s_rtc_wake_elapsed_ms = RTC_TOUCH_POLL_ELAPSED_MS;
    }
    else {
        /* RTC 短周期配置失败时退回 1 秒周期，绝不能在无唤醒源时进入 STOP。 */
        (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        (void)HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_NORMAL_WAKE_COUNTER,
                                         RTC_WAKEUPCLOCK_RTCCLK_DIV16);
        s_rtc_wake_elapsed_ms = 1000U;
    }
}

void PowerManager_EndStopSession(void)
{
    /* 离开 STOP 会话后恢复普通 1 秒 RTC 周期，降低无操作时的唤醒次数。 */
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    (void)HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_NORMAL_WAKE_COUNTER,
                                     RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    s_rtc_wake_elapsed_ms = 1000U;
}

void PowerManager_SuspendUnusedPeripherals(uint8_t for_stop)
{
    GPIO_InitTypeDef gpio = {0};

    /* suspend_level 让暂停操作幂等，避免重复 DeInit 同一外设。 */
    if(s_peripheral_suspend_level != 0U) return;

    s_peripheral_suspend_level = 1U;
    /* AOD 保留 BLE；真正 STOP 才关闭 UART、ADC、SPI、DMA 和背光 PWM。 */
    if(for_stop == 0U) return;
    BLEManager_Suspend();
    (void)HAL_UART_DeInit(&huart1);
    (void)HAL_ADC_DeInit(&hadc1);

    /* 调用前 LCD 休眠命令已发送完，之后才能关闭 SPI DMA 请求和时钟。 */
    LL_SPI_DisableDMAReq_TX(SPI1);
    LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_2);
    NVIC_DisableIRQ(DMA2_Stream2_IRQn);
    LL_SPI_Disable(SPI1);
    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);

    LCD_Set_Light(0U);
    (void)HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
    (void)HAL_TIM_PWM_DeInit(&htim3);
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = LCD_BLK_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_BLK_PORT, &gpio);
    HAL_GPIO_WritePin(LCD_BLK_PORT, LCD_BLK_PIN, GPIO_PIN_RESET);

    __HAL_RCC_DMA2_CLK_DISABLE();
    s_peripheral_suspend_level = 2U;
}

void PowerManager_ResumePeripherals(void)
{
    /* 按暂停层级逆序恢复 CubeMX 外设；AOD 的 level=1 无需重建硬件。 */
    uint8_t level = s_peripheral_suspend_level;

    if(level == 0U) return;
    if(level >= 2U) {
        MX_DMA_Init();
        MX_SPI1_Init();
        MX_TIM3_Init();
        LCD_Open_Light();
        MX_ADC1_Init();
        MX_USART1_UART_Init();
        BLEManager_Resume();
    }
    s_peripheral_suspend_level = 0U;
}

uint8_t PowerManager_StopOnce(void)
{
    uint8_t flags;

    /* 清标志和挂起 SysTick 必须与进入 WFI 原子衔接，避免丢失唤醒事件。 */
    __disable_irq();
    s_wake_flags = 0U;
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* 先开中断执行待处理回调，回调会把具体唤醒源 OR 进 s_wake_flags。 */
    __enable_irq();
    __DSB();
    __ISB();

    /* 重启 PLL 的 HAL 超时循环依赖 tick，因此先恢复 SysTick。 */
    HAL_ResumeTick();
    /* STOP 会把 SYSCLK 切离 PLL，醒来必须恢复完整 100 MHz 时钟树。 */
    SystemClock_Config();

    __disable_irq();
    flags = s_wake_flags;
    s_wake_flags = 0U;
    if((flags & POWER_WAKE_RTC) != 0U) {
        /* STOP 中 SysTick 停止，手动补偿睡眠时间，维持软件 deadline 单调推进。 */
        uwTick += s_rtc_wake_elapsed_ms;
    }
    __enable_irq();

    /* RTC 本身只是轮询节拍；确认手指/手势有效后才追加 TOUCH 唤醒原因。 */
    if((flags & POWER_WAKE_RTC) != 0U) {
        uint8_t finger = CST816_Get_FingerNum();
        uint8_t gesture = CST816_IIC_ReadREG(GestureID);
        if(((finger != 0U) && (finger != 0xFFU)) ||
           ((gesture != NOGESTURE) && (gesture != 0xFFU))) {
            flags |= POWER_WAKE_TOUCH;
        }
    }
    return flags;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    /* 中断中只记录位图，不执行 I2C/LCD/UI 等耗时操作。 */
    if(gpio_pin == KEY1_Pin) {
        s_wake_flags |= POWER_WAKE_KEY;
    }
    else if(gpio_pin == GPIO_PIN_12) {
        s_wake_flags |= POWER_WAKE_MPU;
    }
    else if(gpio_pin == TOUCH_INT_PIN) {
        s_wake_flags |= POWER_WAKE_TOUCH;
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *rtc)
{
    /* HAL 回调可能服务多个 RTC 实例，先确认是本项目 hrtc。 */
    if(rtc == &hrtc) {
        s_wake_flags |= POWER_WAKE_RTC;
    }
}
