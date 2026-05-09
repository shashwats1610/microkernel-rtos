/**
 * @file    ota_client.h
 * @brief   UART-bridged OTA receiver.
 *
 * Wire protocol (little-endian throughout):
 *
 *   Frame:    [SOH(1)=0x01][OP(1)][SEQ(2)][LEN(2)][DATA(LEN)][CRC32(4)]
 *   Response: [ACK(1)=0x06 | NAK(1)=0x15][SEQ(2)]
 *
 *   CRC32 covers OP || SEQ || LEN || DATA (i.e. everything after SOH
 *   except the trailing CRC field). Standard IEEE 802.3 polynomial.
 *
 * Opcodes:
 *   OP_START (0x21): begin a transfer to the inactive slot. DATA carries
 *                    a 4-byte little-endian total_size (the signed image
 *                    length). The receiver erases the inactive slot and
 *                    sets up streaming write state.
 *   OP_DATA  (0x22): a chunk to be appended at the current write offset.
 *                    SEQ must equal the previously ack'd SEQ + 1.
 *   OP_END   (0x23): no DATA. The receiver runs crypto_verify_firmware()
 *                    on the freshly-written slot, sets the OTA-pending
 *                    flag in SharedBootBlock, and reboots into the
 *                    bootloader (which will commit the swap).
 *   OP_ABORT (0x2F): no DATA. Cancels any in-progress transfer.
 *
 * Sequence numbering is 16-bit, wraps at 0xFFFF, starts at 1 for the first
 * START frame. ACKs always echo the received SEQ. NAK indicates a CRC,
 * sequencing, or flash-write failure - the host is expected to retransmit.
 */
#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#define OTA_FRAME_SOH       0x01U
#define OTA_RESP_ACK        0x06U
#define OTA_RESP_NAK        0x15U

#define OTA_OP_START        0x21U
#define OTA_OP_DATA         0x22U
#define OTA_OP_END          0x23U
#define OTA_OP_ABORT        0x2FU

#define OTA_MAX_PAYLOAD     1024U
#define OTA_FRAME_OVERHEAD  10U   /* SOH+OP+SEQ+LEN+CRC32 */
#define OTA_MAX_FRAME       (OTA_FRAME_OVERHEAD + OTA_MAX_PAYLOAD)

/**
 * @brief Run the OTA receiver until either:
 *          - an OP_END causes a system reset, or
 *          - the transfer aborts and the function returns to the caller.
 *
 * Used both as the main OTA-update path (called by recovery_mode_run) and
 * as the receive primitive in the recovery menu's Upload command.
 *
 * @return  true if a complete signed image was received and verified
 *          (caller is expected to set the SharedBootBlock.ota_pending flag
 *          and reset; we do that ourselves before returning true).
 *          false if the transfer was aborted or otherwise failed.
 */
bool ota_client_receive(void);

#endif /* OTA_CLIENT_H */
