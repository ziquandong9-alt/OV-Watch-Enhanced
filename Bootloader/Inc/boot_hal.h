#ifndef BOOT_HAL_H
#define BOOT_HAL_H

#include "boot_manifest.h"
#include <stdint.h>

void BootHw_Init(void);
uint32_t BootHw_Millis(void);
void BootHw_Delay(uint32_t milliseconds);
uint8_t BootHw_KeyPressed(void);
uint8_t BootHw_UartRead(uint8_t *data, uint16_t length, uint32_t timeout_ms);
void BootHw_UartWrite(const uint8_t *data, uint16_t length);
void BootHw_ReportCode(uint8_t code);
uint8_t BootHw_EraseApplication(void);
uint8_t BootHw_WriteImage(uint32_t offset, const uint8_t *data, uint16_t length);
uint8_t BootHw_CommitManifest(const BootManifest_t *manifest);
void BootHw_JumpToApplication(void);
void BootHw_Reset(void);

#endif /* BOOT_HAL_H */
