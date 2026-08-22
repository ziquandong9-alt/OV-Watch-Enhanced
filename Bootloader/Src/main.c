#include "boot_config.h"
#include "boot_crypto.h"
#include "boot_display.h"
#include "boot_hal.h"
#include "boot_manifest.h"
#include "boot_protocol.h"

#include <stdint.h>

static uint8_t DetectDoubleClick(void)
{
    uint32_t started_at = BootHw_Millis();
    uint32_t raw_changed_at = started_at;
    uint8_t raw_pressed = BootHw_KeyPressed();
    uint8_t stable_pressed = raw_pressed;
    uint8_t press_count = 0U;

    while((uint32_t)(BootHw_Millis() - started_at) < BOOT_ENTRY_WINDOW_MS) {
        uint32_t now = BootHw_Millis();
        uint8_t current = BootHw_KeyPressed();

        BootDisplay_SetStartupProgress((uint32_t)(now - started_at),
                                       BOOT_ENTRY_WINDOW_MS);

        if(current != raw_pressed) {
            raw_pressed = current;
            raw_changed_at = now;
        }
        else if((current != stable_pressed) &&
                ((uint32_t)(now - raw_changed_at) >= BOOT_KEY_DEBOUNCE_MS)) {
            stable_pressed = current;
            if(stable_pressed != 0U) {
                ++press_count;
                if(press_count >= 2U) return 1U;
            }
        }
        BootHw_Delay(5U);
    }
    return 0U;
}

int main(void)
{
    const BootManifest_t *manifest =
        (const BootManifest_t *)BOOT_MANIFEST_ADDRESS;
    uint8_t manifest_valid;

    BootHw_Init();
    BootDisplay_Init();
    BootDisplay_ShowStartup();
    manifest_valid = BootCrypto_ValidateManifest(manifest, 1U);

    if(manifest_valid == 0U) {
        BootDisplay_ShowRecovery("FIRMWARE INVALID");
    }
    else if(DetectDoubleClick() != 0U) {
        BootDisplay_ShowRecovery("KEY1 REQUESTED");
    }
    else if(BootCrypto_ValidateVector(manifest) == 0U) {
        BootDisplay_ShowRecovery("VECTOR INVALID");
    }
    else if(BootCrypto_VerifyImage(
                manifest, (const uint8_t *)BOOT_APPLICATION_ADDRESS) == 0U) {
        BootDisplay_ShowRecovery("HASH MISMATCH");
    }
    else {
        BootDisplay_ShowVerified();
        BootHw_Delay(180U);
        BootDisplay_Deinit();
        BootHw_JumpToApplication();
        BootHw_ReportCode(5U); /* Reset Handler 意外返回。正常情况下不可能发生。 */
    }

    /* Invalid or interrupted application images can never escape recovery mode. */
    BootProtocol_Run();
    for(;;) { }
}
