#include "boot_protocol.h"

#include "boot_config.h"
#include "boot_crc32.h"
#include "boot_crypto.h"
#include "boot_display.h"
#include "boot_hal.h"
#include "boot_manifest.h"

#include <stdint.h>

#define OTA_CHUNK_MAGIC 0x4843564FUL /* little-endian "OVCH" */

typedef BOOT_PACKED struct {
    uint32_t magic;
    uint32_t offset;
    uint16_t length;
    uint16_t reserved;
    uint32_t payload_crc32;
} OtaChunkHeader_t;

typedef char OtaChunkHeader_SizeMustBe16[
    (sizeof(OtaChunkHeader_t) == 16U) ? 1 : -1];

enum {
    OTA_ERROR_BAD_HEADER = 1U,
    OTA_ERROR_ERASE = 2U,
    OTA_ERROR_PACKET = 3U,
    OTA_ERROR_FLASH = 4U,
    OTA_ERROR_DIGEST = 5U,
    OTA_ERROR_VECTOR = 6U,
    OTA_ERROR_COMMIT = 7U
};

static void SendStatus(const char tag[4], uint32_t value)
{
    uint8_t frame[8];

    frame[0] = (uint8_t)tag[0];
    frame[1] = (uint8_t)tag[1];
    frame[2] = (uint8_t)tag[2];
    frame[3] = (uint8_t)tag[3];
    frame[4] = (uint8_t)value;
    frame[5] = (uint8_t)(value >> 8U);
    frame[6] = (uint8_t)(value >> 16U);
    frame[7] = (uint8_t)(value >> 24U);
    BootHw_UartWrite(frame, sizeof(frame));
}

static void WaitForSync(void)
{
    static const uint8_t sync[] = {'O','V','O','T','A','1','\r','\n'};
    uint8_t matched = 0U;
    uint8_t byte;

    for(;;) {
        if(BootHw_UartRead(&byte, 1U, 1000U) == 0U) continue;
        if(byte == sync[matched]) {
            ++matched;
            if(matched == sizeof(sync)) return;
        }
        else {
            matched = (byte == sync[0]) ? 1U : 0U;
        }
    }
}

void BootProtocol_Run(void)
{
    BootManifest_t manifest;
    OtaChunkHeader_t chunk_header;
    uint8_t payload[BOOT_CHUNK_MAX_SIZE];
    uint32_t expected_offset;

    for(;;) {
        WaitForSync();
        SendStatus("HELO", BOOT_MANIFEST_FORMAT);

        if(BootHw_UartRead((uint8_t *)&manifest,
                           sizeof(manifest),
                           15000U) == 0U ||
           BootCrypto_ValidateManifest(&manifest, 0U) == 0U) {
            SendStatus("FAIL", OTA_ERROR_BAD_HEADER);
            continue;
        }

        SendStatus("HEAD", manifest.image_size);
        BootDisplay_ShowOtaReceiving();
        if(BootHw_EraseApplication() == 0U) {
            SendStatus("FAIL", OTA_ERROR_ERASE);
            continue;
        }
        SendStatus("ERAS", 0U);
        expected_offset = 0U;

        while(expected_offset < manifest.image_size) {
            uint32_t remaining;

            if(BootHw_UartRead((uint8_t *)&chunk_header,
                               sizeof(chunk_header),
                               30000U) == 0U) {
                SendStatus("FAIL", OTA_ERROR_PACKET);
                break;
            }
            if((chunk_header.magic != OTA_CHUNK_MAGIC) ||
               (chunk_header.length == 0U) ||
               (chunk_header.length > BOOT_CHUNK_MAX_SIZE)) {
                SendStatus("FAIL", OTA_ERROR_PACKET);
                break;
            }
            if(BootHw_UartRead(payload, chunk_header.length, 10000U) == 0U) {
                SendStatus("FAIL", OTA_ERROR_PACKET);
                break;
            }
            if(BootCrc32_Calculate(payload, chunk_header.length) !=
               chunk_header.payload_crc32) {
                SendStatus("NEXT", expected_offset);
                continue;
            }
            if(chunk_header.offset != expected_offset) {
                SendStatus("NEXT", expected_offset);
                continue;
            }
            remaining = manifest.image_size - expected_offset;
            if((uint32_t)chunk_header.length > remaining) {
                SendStatus("FAIL", OTA_ERROR_PACKET);
                break;
            }
            if(BootHw_WriteImage(expected_offset,
                                 payload,
                                 chunk_header.length) == 0U) {
                SendStatus("FAIL", OTA_ERROR_FLASH);
                break;
            }
            expected_offset += chunk_header.length;
            BootDisplay_SetOtaProgress(expected_offset, manifest.image_size);
            SendStatus("NEXT", expected_offset);
        }

        if(expected_offset != manifest.image_size) continue;
        if(BootCrypto_VerifyImage(&manifest,
                                  (const uint8_t *)BOOT_APPLICATION_ADDRESS) == 0U) {
            SendStatus("FAIL", OTA_ERROR_DIGEST);
            continue;
        }
        if(BootCrypto_ValidateVector(&manifest) == 0U) {
            SendStatus("FAIL", OTA_ERROR_VECTOR);
            continue;
        }
        if(BootHw_CommitManifest(&manifest) == 0U) {
            SendStatus("FAIL", OTA_ERROR_COMMIT);
            continue;
        }

        SendStatus("DONE", manifest.firmware_version);
        BootDisplay_ShowUpdateComplete();
        BootHw_Delay(700U);
        BootHw_Reset();
    }
}
