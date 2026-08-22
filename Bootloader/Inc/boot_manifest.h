#ifndef BOOT_MANIFEST_H
#define BOOT_MANIFEST_H

#include "boot_config.h"
#include <stdint.h>

#if defined(__CC_ARM)
#define BOOT_PACKED __packed
#else
#define BOOT_PACKED __attribute__((packed))
#endif

/*
 * Bytes 0..55 are the signed manifest prefix. The ECDSA-P256 signature is
 * raw big-endian r||s. CRC32 covers bytes 0..119. The bootloader writes the
 * valid marker last, after the complete image has been hashed and checked.
 */
typedef BOOT_PACKED struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t header_size;
    uint32_t target_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t image_address;
    uint8_t image_sha256[32];
    uint8_t signature[64];
    uint32_t header_crc32;
    uint32_t valid_marker;
} BootManifest_t;

typedef char BootManifest_SizeMustBe128[
    (sizeof(BootManifest_t) == BOOT_MANIFEST_SIZE) ? 1 : -1];

#define BOOT_MANIFEST_SIGNED_PREFIX_SIZE 56U
#define BOOT_MANIFEST_CRC_SIZE           120U

#endif /* BOOT_MANIFEST_H */
