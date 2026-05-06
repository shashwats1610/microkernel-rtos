/**
 * @file semaphore.h
 * @brief Binary and counting semaphores with blocking wait/post.
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct semaphore_s {
    volatile uint32_t count;
    uint32_t max_count;
    struct tcb_s *wait_head;
    struct tcb_s *wait_tail;
} semaphore_t;

/**
 * @brief Initialize counting semaphore.
 *
 * @param sem Semaphore object.
 * @param initial Initial count.
 * @param max_count Maximum count value.
 * @return Status.
 */
rtos_status_t semaphore_init(semaphore_t *sem, uint32_t initial, uint32_t max_count);

/**
 * @brief Initialize binary semaphore (0 or 1).
 *
 * @param sem Semaphore object.
 * @param initially_available True if starting available.
 * @return Status.
 */
rtos_status_t semaphore_init_binary(semaphore_t *sem, bool initially_available);

/**
 * @brief Wait for semaphore (decrement). Blocks when count is zero.
 *
 * @param sem Semaphore object.
 * @param timeout_ticks Timeout in ticks (0 = infinite).
 * @return Status.
 */
rtos_status_t semaphore_wait(semaphore_t *sem, uint32_t timeout_ticks);

/**
 * @brief Post semaphore (increment) up to max_count.
 *
 * @param sem Semaphore object.
 * @return Status.
 */
rtos_status_t semaphore_post(semaphore_t *sem);

#ifdef __cplusplus
}
#endif

#endif /* SEMAPHORE_H */
