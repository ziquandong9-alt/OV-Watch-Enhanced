#include "power.h"
#include "adc.h"
#include "stm32f4xx_hal.h"

#define POWER_PORT GPIOA
#define POWER_PIN GPIO_PIN_3
#define CHARGE_PORT GPIOA
#define CHARGE_PIN GPIO_PIN_2

void Power_Init(void)
{
    /* 配置电池 ADC 输入和低有效充电状态 GPIO。 */
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = POWER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(POWER_PORT, &gpio);
    HAL_GPIO_WritePin(POWER_PORT, POWER_PIN, GPIO_PIN_SET);
    gpio.Pin = CHARGE_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CHARGE_PORT, &gpio);
}

uint16_t Power_ReadBatteryMillivolts(void)
{
    /* 启动 ADC 转换，将分压后的码值还原为电池端毫伏。 */
    uint32_t sum = 0U;
    uint8_t valid = 0U;
    uint8_t i;
    for(i = 0U; i < 16U; i++) {
        if((HAL_ADC_Start(&hadc1) == HAL_OK) &&
           (HAL_ADC_PollForConversion(&hadc1, 3U) == HAL_OK)) {
            sum += HAL_ADC_GetValue(&hadc1);
            valid++;
        }
        (void)HAL_ADC_Stop(&hadc1);
    }
    if(valid == 0U) return 0U;
    return (uint16_t)(((sum / valid) * 6600UL + 2047UL) / 4095UL);
}

uint8_t Power_IsCharging(void)
{
    /* 把充电管理芯片的低有效状态脚统一转换成 0/1。 */
    return (HAL_GPIO_ReadPin(CHARGE_PORT, CHARGE_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

uint8_t Power_VoltageToPercent(uint16_t mv)
{
    /* 分段线性近似锂电池放电平台，比全区间一条直线更合理。 */
    typedef struct { uint16_t mv; uint8_t percent; } Point;
    static const Point curve[] = {
        {3300U, 0U}, {3450U, 5U}, {3680U, 10U}, {3740U, 20U},
        {3770U, 30U}, {3790U, 40U}, {3820U, 50U}, {3870U, 60U},
        {3920U, 70U}, {3980U, 80U}, {4060U, 90U}, {4200U, 100U}
    };
    uint8_t i;
    if(mv <= curve[0].mv) return 0U;
    for(i = 1U; i < (uint8_t)(sizeof(curve) / sizeof(curve[0])); i++) {
        if(mv <= curve[i].mv) {
            uint32_t span_mv = curve[i].mv - curve[i - 1U].mv;
            uint32_t span_pc = curve[i].percent - curve[i - 1U].percent;
            return (uint8_t)(curve[i - 1U].percent +
                   ((uint32_t)(mv - curve[i - 1U].mv) * span_pc) / span_mv);
        }
    }
    return 100U;
}
