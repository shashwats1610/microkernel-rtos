/**
 * @file    crypto.c
 * @brief   ECDSA-P256 / SHA-256 firmware verification, micro-ecc backend.
 */

#include "crypto.h"
#include "crc32.h"
#include "firmware_format.h"
#include "flash_driver.h"
#include "iwdg.h"
#include "memory_map.h"
#include "public_key.h"
#include "sha256.h"

#include "uECC.h"

#include <stddef.h>
#include <string.h>

/* Compile-time sanity: max payload fits in a slot. */
_Static_assert(FIRMWARE_MAX_PAYLOAD_SIZE + FIRMWARE_HEADER_SIZE <= SLOT_SIZE,
               "Header + payload must fit in a slot");

/* Chunk size used to stream slot contents through flash_read() into a
 * stack buffer for hashing/CRC. Picked to balance stack usage against
 * call overhead; a single 1 KB chunk is well under the bootloader's
 * 16 KB stack budget. Routing through flash_read() (rather than direct
 * pointer dereference) is what lets the QEMU_FLASH_SIM backend serve
 * the shadowed ROM regions transparently. */
#define VERIFY_CHUNK_BYTES   1024U

/**
 * @brief Compute SHA-256 over the canonical signed region:
 *        [header with .signature zeroed || payload].
 *
 * The slot is streamed in 1 KB chunks via `flash_read()`, never via
 * direct pointer dereference, so it works identically on real flash and
 * on the QEMU SRAM-shadow.
 */
static bool hash_signed_region(uint32_t slot_addr,
                               const FirmwareHeader_t *hdr,
                               uint8_t out[32])
{
    static const uint8_t zero_sig[FIRMWARE_SIGNATURE_SIZE] = { 0 };

    /* Offset of the signature field inside FirmwareHeader_t. The fields
     * preceding it are: magic(4) + version(4) + image_size(4) + timestamp(4)
     * + crc32(4) = 20. */
    const size_t sig_offset = 20U;
    _Static_assert(offsetof(FirmwareHeader_t, signature) == 20U,
                   "Signature field offset assumption broken");

    sha256_ctx_t ctx;
    sha256_init(&ctx);

    uint8_t buf[VERIFY_CHUNK_BYTES];

    /* Header bytes before the signature, then 64 zero bytes for the
     * signature, then the rest of the header. We feed them straight
     * from `hdr` because the caller already produced a stable copy. */
    const uint8_t *hdr_bytes = (const uint8_t *)hdr;
    sha256_update(&ctx, hdr_bytes, sig_offset);
    sha256_update(&ctx, zero_sig, sizeof(zero_sig));
    sha256_update(&ctx,
                  hdr_bytes + sig_offset + FIRMWARE_SIGNATURE_SIZE,
                  FIRMWARE_HEADER_SIZE - sig_offset - FIRMWARE_SIGNATURE_SIZE);

    /* Payload, streamed in chunks. */
    uint32_t payload_addr = slot_addr + FIRMWARE_HEADER_SIZE;
    uint32_t remaining    = hdr->image_size;
    while (remaining > 0U) {
        uint32_t chunk = remaining > VERIFY_CHUNK_BYTES
                         ? VERIFY_CHUNK_BYTES : remaining;
        if (flash_read(payload_addr, buf, chunk) != FLASH_OK) return false;
        sha256_update(&ctx, buf, chunk);
        iwdg_kick();
        payload_addr += chunk;
        remaining    -= chunk;
    }
    sha256_final(&ctx, out);
    return true;
}

bool crypto_verify_firmware(uint32_t slot_addr)
{
    /* Pull the header into a stack copy via flash_read() so we don't keep
     * dereferencing a base pointer that may straddle the QEMU shadow / ROM
     * boundary on long payloads. On real hardware this just memcpy's the
     * header bytes from memory-mapped flash. */
    FirmwareHeader_t hdr;
    if (flash_read(slot_addr, (uint8_t *)&hdr, sizeof(hdr)) != FLASH_OK) {
        return false;
    }

    /* (1) Magic. */
    if (hdr.magic != FIRMWARE_MAGIC) return false;

    /* (2) Image size sanity: payload must be > 0 and fit in the slot. */
    if (hdr.image_size == 0U) return false;
    if (hdr.image_size > FIRMWARE_MAX_PAYLOAD_SIZE) return false;

    /* (3) CRC32 fast reject - covers payload only, matches signer
     * convention. Stream the payload in 1 KB chunks. */
    uint32_t crc_state    = CRC32_INIT;
    uint32_t payload_addr = slot_addr + FIRMWARE_HEADER_SIZE;
    uint32_t remaining    = hdr.image_size;
    uint8_t  buf[VERIFY_CHUNK_BYTES];
    while (remaining > 0U) {
        uint32_t chunk = remaining > VERIFY_CHUNK_BYTES
                         ? VERIFY_CHUNK_BYTES : remaining;
        if (flash_read(payload_addr, buf, chunk) != FLASH_OK) return false;
        crc_state = crc32_update(crc_state, buf, chunk);
        iwdg_kick();
        payload_addr += chunk;
        remaining    -= chunk;
    }
    if (crc32_finalize(crc_state) != hdr.crc32) return false;

    /* (4) SHA-256 over [header(zero-sig) || payload]. */
    uint8_t hash[32];
    if (!hash_signed_region(slot_addr, &hdr, hash)) return false;

    /* (5) ECDSA-P256 verify with embedded public key. */
    iwdg_kick();
    int ok = uECC_verify(public_key_xy,
                         hash, sizeof(hash),
                         hdr.signature,
                         uECC_secp256r1());
    iwdg_kick();
    return (ok == 1);
}
