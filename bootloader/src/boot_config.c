/**
 * @file    boot_config.c
 * @brief   Persistent boot configuration storage at BOOT_CONFIG_ADDR.
 */

#include "config.h"
#include "crc32.h"
#include "flash_driver.h"
#include "memory_map.h"

#include <stddef.h>
#include <string.h>

/* Bytes [0, CRC_OFFSET) are protected by the CRC; the CRC field follows. */
#define CFG_CRC_OFFSET   20U
_Static_assert(CFG_CRC_OFFSET == offsetof(BootConfig_t, crc32),
               "CRC offset must equal the offset of the crc32 field");

void boot_config_default(BootConfig_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->slot_a_version    = 0U;
    cfg->slot_b_version    = 0U;
    cfg->active_slot       = SLOT_A;
    cfg->boot_attempts     = 0U;
    cfg->rollback_counter  = 0U;
    cfg->_reserved         = 0U;
    cfg->last_good_boot    = 0U;
    cfg->magic             = BOOT_CONFIG_MAGIC;
    boot_config_stamp_crc(cfg);
}

void boot_config_stamp_crc(BootConfig_t *cfg)
{
    cfg->crc32 = crc32(cfg, CFG_CRC_OFFSET);
}

bool boot_config_validate(const BootConfig_t *cfg)
{
    if (cfg->magic != BOOT_CONFIG_MAGIC) return false;
    uint32_t calc = crc32(cfg, CFG_CRC_OFFSET);
    return (calc == cfg->crc32);
}

bool boot_config_load(BootConfig_t *out)
{
    flash_read(BOOT_CONFIG_ADDR, (uint8_t *)out, sizeof(*out));
    if (boot_config_validate(out)) return true;
    boot_config_default(out);
    return false;
}

uint32_t boot_config_antidowngrade_floor(const BootConfig_t *cfg)
{
    uint32_t floor = (uint32_t)cfg->rollback_counter << 16;
    if (cfg->slot_a_version > floor) {
        floor = cfg->slot_a_version;
    }
    if (cfg->slot_b_version > floor) {
        floor = cfg->slot_b_version;
    }
    return floor;
}

bool boot_config_firmware_allowed(const BootConfig_t *cfg, uint32_t fw_version)
{
    return fw_version >= boot_config_antidowngrade_floor(cfg);
}

void boot_config_record_ota_to_slot(BootConfig_t *cfg,
                                    uint8_t slot,
                                    uint32_t fw_version)
{
    const uint32_t floor = boot_config_antidowngrade_floor(cfg);
    if (slot == SLOT_A) {
        cfg->slot_a_version = fw_version;
    } else {
        cfg->slot_b_version = fw_version;
    }
    if (fw_version > floor) {
        const uint8_t major = (uint8_t)((fw_version >> 16) & 0xFFU);
        cfg->rollback_counter = (major != 0U) ? major : 1U;
    }
}

bool boot_config_save(const BootConfig_t *cfg)
{
    /* Defensive copy: stamp the CRC ourselves so callers can't pass a
     * mismatched record by accident. */
    BootConfig_t buf = *cfg;
    boot_config_stamp_crc(&buf);

    /*
     * Step 1: erase. On QEMU this clears the simulated sector to 0xFF.
     * On real STM32F4 this erases the whole 64 KB sector 4 (the spec calls
     * out the limitation in docs/architecture.md and security_analysis.md).
     */
    if (flash_erase_range(BOOT_CONFIG_ADDR, sizeof(buf)) != FLASH_OK) {
        return false;
    }

    /*
     * Step 2: program everything EXCEPT the CRC32 field.
     */
    if (flash_program_bytes(BOOT_CONFIG_ADDR,
                            (const uint8_t *)&buf,
                            CFG_CRC_OFFSET) != FLASH_OK) {
        return false;
    }

    /*
     * Step 3: program the CRC field last, as a single 32-bit word.
     * If power is lost between step 2 and step 3, the on-flash record is
     * left CRC-invalid (the erased CRC slot reads as 0xFFFFFFFF) and the
     * next boot rejects it -> falls back to defaults.
     */
    if (flash_program_word(BOOT_CONFIG_ADDR + CFG_CRC_OFFSET,
                           buf.crc32) != FLASH_OK) {
        return false;
    }

    /*
     * Step 4: full readback verify.
     */
    BootConfig_t readback;
    flash_read(BOOT_CONFIG_ADDR, (uint8_t *)&readback, sizeof(readback));
    if (memcmp(&readback, &buf, sizeof(buf)) != 0) {
        return false;
    }
    return true;
}
