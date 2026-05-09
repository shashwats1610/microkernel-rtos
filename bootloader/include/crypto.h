/**
 * @file    crypto.h
 * @brief   Firmware verification API (ECDSA P-256 over SHA-256).
 *
 * The on-device backend is micro-ecc; mbedTLS is the documented production
 * alternative (see docs/security_analysis.md). The function signatures here
 * are deliberately backend-agnostic so swapping is mechanical.
 */
#ifndef BOOTLOADER_CRYPTO_H
#define BOOTLOADER_CRYPTO_H

#include <stdbool.h>
#include <stdint.h>

#include "firmware_format.h"

/**
 * @brief Verify a signed firmware image at @p slot_addr.
 *
 * Sequence:
 *   1. Magic check (FirmwareHeader_t.magic == 0xDEADBEEF).
 *   2. Image-size sanity (must fit inside SLOT_SIZE - sizeof(header)).
 *   3. CRC32 fast pre-check (cheap reject for corrupt or wrong-key images).
 *   4. SHA-256 over [header_with_signature_field_zeroed || payload].
 *   5. ECDSA-P256 verify against the embedded public key.
 *
 * The function calls iwdg_kick() at multiple points; the longest single
 * uninterrupted block is the ECDSA point-mult inside uECC, which is well
 * under the IWDG period.
 *
 * @return  true iff every check passed.
 */
bool crypto_verify_firmware(uint32_t slot_addr);

#endif /* BOOTLOADER_CRYPTO_H */
