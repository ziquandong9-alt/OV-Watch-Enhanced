#include "boot_crc32.h"

/* Ethernet/ZIP CRC-32, reflected polynomial 0xEDB88320. */
uint32_t BootCrc32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t index;
    uint8_t bit;

    for(index = 0U; index < length; ++index) {
        crc ^= data[index];
        for(bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}
