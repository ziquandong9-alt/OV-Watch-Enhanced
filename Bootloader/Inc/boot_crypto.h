#ifndef BOOT_CRYPTO_H
#define BOOT_CRYPTO_H

#include "boot_manifest.h"
#include <stdint.h>

uint8_t BootCrypto_ValidateManifest(const BootManifest_t *manifest,
                                    uint8_t require_commit_marker);
uint8_t BootCrypto_VerifyImage(const BootManifest_t *manifest,
                               const uint8_t *image);
uint8_t BootCrypto_ValidateVector(const BootManifest_t *manifest);

#endif /* BOOT_CRYPTO_H */
