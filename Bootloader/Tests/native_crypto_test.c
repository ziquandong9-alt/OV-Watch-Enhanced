#include "uECC.h"
#include "tinycrypt/sha256.h"
#include "ota_public_key.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t Crc32(const uint8_t *data, uint32_t length)
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

static uint32_t ReadLe32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

int main(int argc, char **argv)
{
    FILE *file;
    uint8_t manifest[128];
    uint8_t *image;
    uint8_t digest[32];
    uint32_t image_size;
    struct tc_sha256_state_struct state;

    if(argc != 3) return 10;
    file = fopen(argv[1], "rb");
    if(file == NULL || fread(manifest, 1U, sizeof(manifest), file) != sizeof(manifest)) return 11;
    fclose(file);
    image_size = ReadLe32(&manifest[16]);
    image = (uint8_t *)malloc(image_size);
    if(image == NULL) return 12;
    file = fopen(argv[2], "rb");
    if(file == NULL || fread(image, 1U, image_size, file) != image_size) return 13;
    fclose(file);

    if(Crc32(manifest, 120U) != ReadLe32(&manifest[120])) return 20;
    if(tc_sha256_init(&state) == 0 || tc_sha256_update(&state, manifest, 56U) == 0 ||
       tc_sha256_final(digest, &state) == 0) return 21;
    if(uECC_verify(g_ota_public_key_p256, digest, sizeof(digest),
                   &manifest[56], uECC_secp256r1()) != 1) return 22;
    if(tc_sha256_init(&state) == 0 || tc_sha256_update(&state, image, image_size) == 0 ||
       tc_sha256_final(digest, &state) == 0) return 23;
    if(memcmp(digest, &manifest[24], sizeof(digest)) != 0) return 24;
    free(image);
    puts("CRC_OK ECDSA_OK IMAGE_SHA256_OK");
    return 0;
}
