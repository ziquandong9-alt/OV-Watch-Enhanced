#include "adc.h"

ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef channel = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if(HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    channel.Channel = ADC_CHANNEL_1;
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    if(HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) Error_Handler();
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adc)
{
    GPIO_InitTypeDef gpio = {0};
    if(adc->Instance != ADC1) return;
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adc)
{
    if(adc->Instance != ADC1) return;
    __HAL_RCC_ADC1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1);
}
