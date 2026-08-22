#include "boot_hal.h"

#include "boot_config.h"
#include "stm32f4xx_hal.h"

static UART_HandleTypeDef s_uart;

/* HAL_Init 使用 SysTick 维护毫秒节拍；独立 Bootloader 工程必须提供自己的中断入口。 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Keep TPS_EN asserted before HAL initialization so releasing the power key is safe. */
static void HoldPowerImmediately(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->BSRR = GPIO_PIN_3;
    GPIOA->MODER = (GPIOA->MODER & ~(3UL << (3U * 2U))) |
                   (1UL << (3U * 2U));
    GPIOA->OTYPER &= ~GPIO_PIN_3;
}

void HAL_UART_MspInit(UART_HandleTypeDef *uart)
{
    GPIO_InitTypeDef gpio = {0};

    if(uart->Instance != USART1) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uart)
{
    if(uart->Instance != USART1) return;
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
}

void BootHw_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    HoldPowerImmediately();
    HAL_Init();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PA3: power latch; PA8: KT6328 enable. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_8, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PB0 是 LCD 背光脚。Bootloader 不初始化 LCD，仅用一次短闪证明自己已运行。 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* PA5 KEY1 is active low. */
    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_uart.Instance = USART1;
    s_uart.Init.BaudRate = BOOT_UART_BAUD_RATE;
    s_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits = UART_STOPBITS_1;
    s_uart.Init.Parity = UART_PARITY_NONE;
    s_uart.Init.Mode = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(&s_uart);
}

uint32_t BootHw_Millis(void) { return HAL_GetTick(); }
void BootHw_Delay(uint32_t milliseconds) { HAL_Delay(milliseconds); }

uint8_t BootHw_KeyPressed(void)
{
    return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) ? 1U : 0U;
}

uint8_t BootHw_UartRead(uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    return (HAL_UART_Receive(&s_uart, data, length, timeout_ms) == HAL_OK) ? 1U : 0U;
}

void BootHw_UartWrite(const uint8_t *data, uint16_t length)
{
    (void)HAL_UART_Transmit(&s_uart, (uint8_t *)data, length, 5000U);
}

void BootHw_ReportCode(uint8_t code)
{
    uint8_t repeat;
    uint8_t pulse;

    /* 故障码重复三轮；忽略上电时独立的 100 ms 快闪，只数这里的慢闪。 */
    for(repeat = 0U; repeat < 3U; ++repeat) {
        HAL_Delay(700U);
        for(pulse = 0U; pulse < code; ++pulse) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
            HAL_Delay(280U);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
            HAL_Delay(280U);
        }
    }
}

uint8_t BootHw_EraseApplication(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error_sector = 0U;
    HAL_StatusTypeDef status;

    if(HAL_FLASH_Unlock() != HAL_OK) return 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_4;
    erase.NbSectors = 4U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    status = HAL_FLASHEx_Erase(&erase, &error_sector);
    (void)HAL_FLASH_Lock();
    return (status == HAL_OK) ? 1U : 0U;
}

uint8_t BootHw_WriteImage(uint32_t offset, const uint8_t *data, uint16_t length)
{
    uint32_t address = BOOT_APPLICATION_ADDRESS + offset;
    uint16_t consumed = 0U;
    HAL_StatusTypeDef status = HAL_OK;

    if((data == NULL) || (length == 0U)) return 0U;
    if((offset > BOOT_APPLICATION_MAX_SIZE) ||
       ((uint32_t)length > (BOOT_APPLICATION_MAX_SIZE - offset))) return 0U;
    if((address & 3U) != 0U) return 0U;
    if(HAL_FLASH_Unlock() != HAL_OK) return 0U;

    while(consumed < length) {
        uint32_t word = 0xFFFFFFFFUL;
        uint8_t copy_count = (uint8_t)(((uint16_t)(length - consumed) >= 4U) ? 4U :
                                       (uint16_t)(length - consumed));
        uint8_t index;

        for(index = 0U; index < copy_count; ++index) {
            word &= ~(0xFFUL << (index * 8U));
            word |= (uint32_t)data[consumed + index] << (index * 8U);
        }
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word);
        if(status != HAL_OK) break;
        if(*(const uint32_t *)address != word) { status = HAL_ERROR; break; }
        address += 4U;
        consumed = (uint16_t)(consumed + copy_count);
    }

    (void)HAL_FLASH_Lock();
    return (status == HAL_OK) ? 1U : 0U;
}

static uint8_t ProgramWords(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t offset;

    for(offset = 0U; offset < length; offset += 4U) {
        uint32_t word = (uint32_t)data[offset] |
                        ((uint32_t)data[offset + 1U] << 8U) |
                        ((uint32_t)data[offset + 2U] << 16U) |
                        ((uint32_t)data[offset + 3U] << 24U);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                             address + offset,
                             word) != HAL_OK) return 0U;
        if(*(const uint32_t *)(address + offset) != word) return 0U;
    }
    return 1U;
}

uint8_t BootHw_CommitManifest(const BootManifest_t *manifest)
{
    BootManifest_t stored;
    uint32_t marker = BOOT_MANIFEST_VALID_MARKER;
    uint8_t result;

    if(manifest == NULL) return 0U;
    stored = *manifest;
    stored.valid_marker = 0xFFFFFFFFUL;

    if(HAL_FLASH_Unlock() != HAL_OK) return 0U;
    result = ProgramWords(BOOT_MANIFEST_ADDRESS,
                          (const uint8_t *)&stored,
                          BOOT_MANIFEST_SIZE - 4U);
    if(result != 0U) {
        result = ProgramWords(BOOT_MANIFEST_ADDRESS + BOOT_MANIFEST_SIZE - 4U,
                              (const uint8_t *)&marker,
                              4U);
    }
    (void)HAL_FLASH_Lock();
    return result;
}

void BootHw_JumpToApplication(void)
{
    const uint32_t *vectors = (const uint32_t *)BOOT_APPLICATION_ADDRESS;
    void (*application_reset)(void) = (void (*)(void))vectors[1];
    uint32_t index;

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for(index = 0U; index < 8U; ++index) {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }
    (void)HAL_UART_DeInit(&s_uart);
    SCB->VTOR = BOOT_APPLICATION_ADDRESS;
    __DSB();
    __ISB();
    __set_CONTROL(0U);
    __set_MSP(vectors[0]);
    __enable_irq();
    application_reset();
    for(;;) { }
}

void BootHw_Reset(void)
{
    NVIC_SystemReset();
}
