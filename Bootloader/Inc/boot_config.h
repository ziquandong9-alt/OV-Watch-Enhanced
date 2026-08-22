#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include <stdint.h>

/* STM32F411CEU6 internal Flash: 512 KiB, sectors 0..7. */
#define BOOT_FLASH_BASE              0x08000000UL
#define BOOT_FLASH_END               0x08080000UL
#define BOOT_REGION_SIZE             0x00010000UL /* sectors 0..3, 64 KiB */

/* Sector 4 starts with a 4 KiB manifest page; the relocated application follows. */
#define BOOT_MANIFEST_ADDRESS        0x08010000UL
#define BOOT_APPLICATION_ADDRESS     0x08011000UL
#define BOOT_APPLICATION_MAX_SIZE    (BOOT_FLASH_END - BOOT_APPLICATION_ADDRESS)

#define BOOT_SRAM_BASE               0x20000000UL
#define BOOT_SRAM_END                0x20020000UL

#define BOOT_MANIFEST_MAGIC          0x314D564FUL /* little-endian bytes: "OVM1" */
#define BOOT_MANIFEST_FORMAT         1U
#define BOOT_MANIFEST_SIZE           128U
#define BOOT_MANIFEST_VALID_MARKER   0x51A7B007UL
#define BOOT_TARGET_STM32F411CE      0xF411CE01UL

#define BOOT_ENTRY_WINDOW_MS         3000UL
#define BOOT_KEY_DEBOUNCE_MS         30UL
#define BOOT_UART_BAUD_RATE          9600UL
#define BOOT_CHUNK_MAX_SIZE          256U

#endif /* BOOT_CONFIG_H */
