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

rtos_status_t semaphore_init(semaphore_t *sem, uint32_t initial, uint32_t max_count);
rtos_status_t semaphore_init_binary(semaphore_t *sem, bool initially_available);
rtos_status_t semaphore_wait(semaphore_t *sem, uint32_t timeout_ticks);
rtos_status_t semaphore_post(semaphore_t *sem);

void semaphore_wake_timeout(struct tcb_s *t);
void semaphore_detach_waiter(semaphore_t *sem, struct tcb_s *t);

#ifdef __cplusplus
}
#endif

#endif /* SEMAPHORE_H */
