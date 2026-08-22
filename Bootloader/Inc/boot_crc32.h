#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

#include <stdint.h>

uint32_t BootCrc32_Calculate(const uint8_t *data, uint32_t length);

#endif /* BOOT_CRC32_H */
