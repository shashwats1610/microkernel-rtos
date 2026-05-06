/**
 * @file mutex.h
 * @brief Mutex with optional priority inheritance.
 */

#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mutex_s {
    volatile bool locked;
    struct tcb_s *owner;
    struct tcb_s *wait_head;
    struct tcb_s *wait_tail;
} mutex_t;

/**
 * @brief Initialize mutex object (not locked).
 *
 * @param m Mutex instance.
 * @return Status.
 */
rtos_status_t mutex_init(mutex_t *m);

/**
 * @brief Lock mutex (blocking with optional PI).
 *
 * @param m Mutex instance.
 * @param timeout_ticks Wait timeout in ticks (0 = wait forever).
 * @return Status.
 */
rtos_status_t mutex_lock(mutex_t *m, uint32_t timeout_ticks);

/**
 * @brief Unlock mutex held by current task.
 *
 * @param m Mutex instance.
 * @return Status.
 */
rtos_status_t mutex_unlock(mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* MUTEX_H */
