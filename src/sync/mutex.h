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

rtos_status_t mutex_init(mutex_t *m);
rtos_status_t mutex_lock(mutex_t *m, uint32_t timeout_ticks);
rtos_status_t mutex_unlock(mutex_t *m);

/** @brief Remove waiter on timeout (called from tick wake). */
void mutex_wake_timeout(struct tcb_s *t);

/** @brief Detach task from mutex wait list (task delete). */
void mutex_detach_waiter(mutex_t *m, struct tcb_s *t);

#ifdef __cplusplus
}
#endif

#endif /* MUTEX_H */
