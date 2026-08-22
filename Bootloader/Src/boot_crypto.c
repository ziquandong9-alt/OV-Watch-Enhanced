#include "boot_crypto.h"

#include "boot_crc32.h"
#include "ota_public_key.h"
#include "uECC.h"
#include "tinycrypt/sha256.h"

static uint8_t ConstantTimeEqual(const uint8_t *left,
                                 const uint8_t *right,
                                 uint32_t length)
{
    uint8_t difference = 0U;
    uint32_t index;

    for(index = 0U; index < length; ++index) difference |= left[index] ^ right[index];
    return (difference == 0U) ? 1U : 0U;
}

static uint8_t Sha256(const uint8_t *data, uint32_t length, uint8_t digest[32])
{
    struct tc_sha256_state_struct state;

    if(tc_sha256_init(&state) == 0) return 0U;
    if(tc_sha256_update(&state, data, (size_t)length) == 0) return 0U;
    if(tc_sha256_final(digest, &state) == 0) return 0U;
    return 1U;
}

uint8_t BootCrypto_ValidateManifest(const BootManifest_t *manifest,
                                    uint8_t require_commit_marker)
{
    uint8_t prefix_hash[32];
    uECC_Curve curve = uECC_secp256r1();

    if(manifest == NULL) return 0U;
    if(manifest->magic != BOOT_MANIFEST_MAGIC) return 0U;
    if(manifest->format_version != BOOT_MANIFEST_FORMAT) return 0U;
    if(manifest->header_size != BOOT_MANIFEST_SIZE) return 0U;
    if(manifest->target_id != BOOT_TARGET_STM32F411CE) return 0U;
    if(manifest->image_address != BOOT_APPLICATION_ADDRESS) return 0U;
    if((manifest->image_size < 8U) ||
       (manifest->image_size > BOOT_APPLICATION_MAX_SIZE)) return 0U;
    if((require_commit_marker != 0U) &&
       (manifest->valid_marker != BOOT_MANIFEST_VALID_MARKER)) return 0U;
    if(BootCrc32_Calculate((const uint8_t *)manifest,
                           BOOT_MANIFEST_CRC_SIZE) != manifest->header_crc32) return 0U;
    if(Sha256((const uint8_t *)manifest,
              BOOT_MANIFEST_SIGNED_PREFIX_SIZE,
              prefix_hash) == 0U) return 0U;

    return (uECC_verify(g_ota_public_key_p256,
                        prefix_hash,
                        sizeof(prefix_hash),
                        manifest->signature,
                        curve) == 1) ? 1U : 0U;
}

uint8_t BootCrypto_VerifyImage(const BootManifest_t *manifest,
                               const uint8_t *image)
{
    uint8_t digest[32];

    if((manifest == NULL) || (image == NULL)) return 0U;
    if(Sha256(image, manifest->image_size, digest) == 0U) return 0U;
    return ConstantTimeEqual(digest, manifest->image_sha256, sizeof(digest));
}

uint8_t BootCrypto_ValidateVector(const BootManifest_t *manifest)
{
    const uint32_t *vectors = (const uint32_t *)BOOT_APPLICATION_ADDRESS;
    uint32_t stack_pointer;
    uint32_t reset_handler;
    uint32_t reset_address;

    if(manifest == NULL) return 0U;
    stack_pointer = vectors[0];
    reset_handler = vectors[1];
    reset_address = reset_handler & ~1UL;

    if((stack_pointer < BOOT_SRAM_BASE) || (stack_pointer > BOOT_SRAM_END)) return 0U;
    if((reset_handler & 1U) == 0U) return 0U;
    if(reset_address < BOOT_APPLICATION_ADDRESS) return 0U;
    if(reset_address >= (BOOT_APPLICATION_ADDRESS + manifest->image_size)) return 0U;
    return 1U;
}
