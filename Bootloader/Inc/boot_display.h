#ifndef BOOT_DISPLAY_H
#define BOOT_DISPLAY_H

#include <stdint.h>

void BootDisplay_Init(void);
void BootDisplay_ShowStartup(void);
void BootDisplay_SetStartupProgress(uint32_t elapsed_ms, uint32_t total_ms);
void BootDisplay_ShowRecovery(const char *reason);
void BootDisplay_ShowOtaReceiving(void);
void BootDisplay_SetOtaProgress(uint32_t received, uint32_t total);
void BootDisplay_ShowUpdateComplete(void);
void BootDisplay_ShowVerified(void);
void BootDisplay_Deinit(void);

#endif /* BOOT_DISPLAY_H */
