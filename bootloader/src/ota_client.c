/**
 * @file    ota_client.c
 * @brief   UART-bridged OTA receiver. See ota_client.h for the wire format.
 */

#include "ota_client.h"

#include "config.h"
#include "crc32.h"
#include "crypto.h"
#include "firmware_format.h"
#include "flash_driver.h"
#include "iwdg.h"
#include "memory_map.h"
#include "shared_memory.h"
#include "uart.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Inter-byte timeout while reading a single frame (host should be back-to-
 * back within ~50 ms even on slow links). */
#define OTA_BYTE_TIMEOUT_MS   500U

/* Forward decl from main.c. */
__attribute__((noreturn)) void bootloader_system_reset(void);

/* ===================================================================== */
/* Per-transfer state                                                       */
/* ===================================================================== */
typedef struct {
    bool      active;        /* between START and END */
    uint32_t  slot_addr;     /* destination slot's base address */
    uint32_t  total_size;    /* expected total image bytes */
    uint32_t  written;       /* bytes already programmed */
    uint16_t  expected_seq;  /* next SEQ we will accept */
} ota_state_t;

static ota_state_t g_state;

static void send_response(uint8_t code, uint16_t seq)
{
    uint8_t buf[3];
    buf[0] = code;
    buf[1] = (uint8_t)(seq & 0xFFU);
    buf[2] = (uint8_t)((seq >> 8) & 0xFFU);
    uart_ota_write(buf, sizeof(buf));
}

static inline uint16_t rd_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t rd_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] <<  8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

/* ===================================================================== */
/* Frame reader                                                             */
/* ===================================================================== */
typedef struct {
    uint8_t  op;
    uint16_t seq;
    uint16_t len;
    uint8_t  data[OTA_MAX_PAYLOAD];
} ota_frame_t;

typedef enum {
    FRAME_OK = 0,
    FRAME_TIMEOUT,
    FRAME_CRC_BAD,
    FRAME_TOO_BIG,
    FRAME_BAD_SOH,
} frame_status_t;

static frame_status_t read_frame(ota_frame_t *frame)
{
    /* Resync to SOH. */
    uint8_t b;
    while (true) {
        if (!uart_ota_getc(&b, OTA_BYTE_TIMEOUT_MS)) return FRAME_TIMEOUT;
        if (b == OTA_FRAME_SOH) break;
        /* Non-SOH bytes are silently dropped (host may resync). */
    }

    uint8_t hdr[5]; /* OP + SEQ(2) + LEN(2) */
    if (uart_ota_read(hdr, sizeof(hdr), OTA_BYTE_TIMEOUT_MS) != sizeof(hdr)) {
        return FRAME_TIMEOUT;
    }
    frame->op  = hdr[0];
    frame->seq = rd_u16_le(&hdr[1]);
    frame->len = rd_u16_le(&hdr[3]);

    if (frame->len > OTA_MAX_PAYLOAD) return FRAME_TOO_BIG;

    if (frame->len > 0U) {
        if (uart_ota_read(frame->data, frame->len, OTA_BYTE_TIMEOUT_MS)
            != frame->len) {
            return FRAME_TIMEOUT;
        }
    }

    uint8_t crc_buf[4];
    if (uart_ota_read(crc_buf, sizeof(crc_buf), OTA_BYTE_TIMEOUT_MS)
        != sizeof(crc_buf)) {
        return FRAME_TIMEOUT;
    }
    uint32_t crc_rx = rd_u32_le(crc_buf);

    /* CRC covers OP || SEQ || LEN || DATA. */
    uint32_t crc = CRC32_INIT;
    crc = crc32_update(crc, hdr, sizeof(hdr));
    if (frame->len) crc = crc32_update(crc, frame->data, frame->len);
    crc = crc32_finalize(crc);
    if (crc != crc_rx) return FRAME_CRC_BAD;

    return FRAME_OK;
}

/* ===================================================================== */
/* Per-opcode handlers                                                      */
/* ===================================================================== */
static bool handle_start(const ota_frame_t *f)
{
    if (f->len < 4U) return false;

    /* Pick the inactive slot from the live boot config. */
    BootConfig_t cfg;
    boot_config_load(&cfg);
    uint8_t target = memory_map_other_slot(cfg.active_slot);
    uint32_t slot_addr = memory_map_slot_addr(target);
    uint32_t total = rd_u32_le(&f->data[0]);

    if (total < FIRMWARE_HEADER_SIZE ||
        total > (FIRMWARE_HEADER_SIZE + FIRMWARE_MAX_PAYLOAD_SIZE)) {
        uart_log_printf("OTA : reject START total_size=%u out of range\r\n",
                        (unsigned)total);
        return false;
    }

    uart_log_printf("OTA : START slot=%c addr=0x%08x total=%u\r\n",
                    (char)target, (unsigned)slot_addr, (unsigned)total);

    if (flash_erase_range(slot_addr, total) != FLASH_OK) {
        uart_log_puts("OTA : erase failed\r\n");
        return false;
    }

    g_state.active        = true;
    g_state.slot_addr     = slot_addr;
    g_state.total_size    = total;
    g_state.written       = 0U;
    g_state.expected_seq  = (uint16_t)(f->seq + 1U);
    return true;
}

static bool handle_data(const ota_frame_t *f)
{
    if (!g_state.active)                            return false;
    if (f->seq != g_state.expected_seq)             return false;
    if (f->len == 0U)                               return false;
    if (g_state.written + f->len > g_state.total_size) return false;

    if (flash_program_bytes(g_state.slot_addr + g_state.written,
                            f->data, f->len) != FLASH_OK) {
        uart_log_puts("OTA : flash write failed\r\n");
        return false;
    }
    g_state.written      += f->len;
    g_state.expected_seq  = (uint16_t)(g_state.expected_seq + 1U);
    return true;
}

static bool handle_end(void)
{
    if (!g_state.active) return false;
    if (g_state.written != g_state.total_size) {
        uart_log_printf("OTA : END short, got %u of %u\r\n",
                        (unsigned)g_state.written,
                        (unsigned)g_state.total_size);
        return false;
    }
    if (!crypto_verify_firmware(g_state.slot_addr)) {
        uart_log_puts("OTA : verify failed; staying on active slot\r\n");
        return false;
    }
    /* Mark OTA pending so the bootloader swaps on the next boot. */
    SHARED_BOOT_BLOCK.magic       = SHARED_BLOCK_MAGIC;
    SHARED_BOOT_BLOCK.ota_pending = 1U;
    SHARED_BOOT_BLOCK.boot_confirmed = 0U;
    uart_log_puts("OTA : END verified; rebooting\r\n");
    return true;
}

/* ===================================================================== */
/* Public entrypoint                                                        */
/* ===================================================================== */
bool ota_client_receive(void)
{
    memset(&g_state, 0, sizeof(g_state));
    uart_log_puts("OTA : ready, awaiting frames on USART2 (TCP 4444 in QEMU)\r\n");

    for (uint32_t consec_errs = 0; consec_errs < 16U; ) {
        iwdg_kick();
        ota_frame_t frame;
        frame_status_t s = read_frame(&frame);
        if (s != FRAME_OK) {
            ++consec_errs;
            /* Best-effort NAK with seq=0 since we don't know the seq. */
            if (s == FRAME_CRC_BAD || s == FRAME_TOO_BIG) {
                send_response(OTA_RESP_NAK, 0U);
            }
            /* Timeout / bad SOH: stay quiet and let host retry. */
            continue;
        }

        bool ok = false;
        switch (frame.op) {
        case OTA_OP_START: ok = handle_start(&frame);          break;
        case OTA_OP_DATA:  ok = handle_data(&frame);           break;
        case OTA_OP_END:   ok = handle_end();                  break;
        case OTA_OP_ABORT:
            uart_log_puts("OTA : ABORT received\r\n");
            send_response(OTA_RESP_ACK, frame.seq);
            return false;
        default:           ok = false;                          break;
        }

        send_response(ok ? OTA_RESP_ACK : OTA_RESP_NAK, frame.seq);
        if (ok) {
            consec_errs = 0;
            if (frame.op == OTA_OP_END) {
                /* Give the ACK time to drain on the wire, then reset. */
                for (volatile uint32_t i = 0; i < 200000U; ++i) { }
                bootloader_system_reset();
                return true;  /* unreachable */
            }
        } else {
            ++consec_errs;
        }
    }

    uart_log_puts("OTA : too many consecutive errors; aborting\r\n");
    return false;
}
