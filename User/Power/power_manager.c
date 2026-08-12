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

#define RTC_NORMAL_WAKE_COUNTER       2048U
#define RTC_TOUCH_POLL_COUNTER        256U
#define RTC_TOUCH_POLL_ELAPSED_MS     125U

extern void SystemClock_Config(void);

void PowerManager_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = KEY1_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY1_GPIO_Port, &gpio);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* TP_INT is not routed to the MCU on the published core PCB. */
    CST816_IIC_WriteREG(DisAutoSleep, 1U);
}

void PowerManager_BeginStopSession(void)
{
    /* Reset stale gesture state, then poll the controller from short RTC wakes. */
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
        /* Never enter STOP without a periodic software-poll fallback. */
        (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        (void)HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_NORMAL_WAKE_COUNTER,
                                         RTC_WAKEUPCLOCK_RTCCLK_DIV16);
        s_rtc_wake_elapsed_ms = 1000U;
    }
}

void PowerManager_EndStopSession(void)
{
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    (void)HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_NORMAL_WAKE_COUNTER,
                                     RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    s_rtc_wake_elapsed_ms = 1000U;
}

void PowerManager_SuspendUnusedPeripherals(uint8_t for_stop)
{
    GPIO_InitTypeDef gpio = {0};

    if(s_peripheral_suspend_level != 0U) return;

    s_peripheral_suspend_level = 1U;
    /* BLE remains available during always-on display, but not true STOP. */
    if(for_stop == 0U) return;
    BLEManager_Suspend();
    (void)HAL_UART_DeInit(&huart1);
    (void)HAL_ADC_DeInit(&hadc1);

    /* The LCD command has already completed before this function is called. */
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

    __disable_irq();
    s_wake_flags = 0U;
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* Service the pending wake IRQ before collecting its callback flag. */
    __enable_irq();
    __DSB();
    __ISB();

    /* HAL RCC timeout loops need the tick before the PLL is restarted. */
    HAL_ResumeTick();
    /* STOP switches SYSCLK away from PLL. Restore the complete 100 MHz tree. */
    SystemClock_Config();

    __disable_irq();
    flags = s_wake_flags;
    s_wake_flags = 0U;
    if((flags & POWER_WAKE_RTC) != 0U) {
        uwTick += s_rtc_wake_elapsed_ms;
    }
    __enable_irq();

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
    if(rtc == &hrtc) {
        s_wake_flags |= POWER_WAKE_RTC;
    }
}
