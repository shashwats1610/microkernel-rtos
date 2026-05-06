/**
 * @file msgqueue.h
 * @brief Fixed-size message queues (FIFO ring buffer).
 */

#ifndef MSGQUEUE_H
#define MSGQUEUE_H

#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_queue_s {
    uint8_t *buffer;
    uint32_t msg_size;
    uint32_t depth;
    uint32_t head;
    uint32_t tail;
    uint32_t count;

    struct tcb_s *send_wait_head;
    struct tcb_s *send_wait_tail;
    struct tcb_s *recv_wait_head;
    struct tcb_s *recv_wait_tail;
} msg_queue_t;

/**
 * @brief Initialize queue using caller-provided storage.
 *
 * @param q Queue object.
 * @param storage Byte storage sized @a depth * @a msg_size.
 * @param msg_size Fixed message size in bytes (>0).
 * @param depth Number of messages (>0).
 * @return Status.
 */
rtos_status_t msg_queue_init(msg_queue_t *q, uint8_t *storage, uint32_t msg_size,
                             uint32_t depth);

/**
 * @brief Blocking send with optional timeout (best-effort).
 *
 * @param q Queue object.
 * @param msg Message pointer copied into queue.
 * @param timeout_ticks Timeout (0 = infinite).
 * @return Status.
 */
rtos_status_t msg_queue_send(msg_queue_t *q, const void *msg, uint32_t timeout_ticks);

/**
 * @brief Blocking receive with optional timeout (best-effort).
 *
 * @param q Queue object.
 * @param out Output buffer sized @a msg_size.
 * @param timeout_ticks Timeout (0 = infinite).
 * @return Status.
 */
rtos_status_t msg_queue_recv(msg_queue_t *q, void *out, uint32_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif /* MSGQUEUE_H */
