/**
 * @file scheduler.h
 * @brief Preemptive priority scheduler API (internal).
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize priority queues and idle bookkeeping.
 */
void scheduler_init(void);

/**
 * @brief Add a task to its priority ready ring.
 *
 * @param tcb Task control block (must be valid).
 * @return Status.
 */
rtos_status_t scheduler_add_task(TCB_t *tcb);

/**
 * @brief Remove a task from the ready ring if present.
 *
 * @param tcb Task control block.
 */
void scheduler_remove_task(TCB_t *tcb);

/**
 * @brief Select next task for execution (called from PendSV only).
 *
 * @return Pointer to next TCB (never NULL when idle exists).
 */
TCB_t *scheduler_get_next(void);

/**
 * @brief Request reschedule if a higher-priority task is ready or RR slice expired.
 */
void scheduler_mark_reschedule(void);

/**
 * @brief Globally runnable pointer used by PendSV (written only from kernel).
 */
extern volatile TCB_t *current_tcb;

/**
 * @brief Idle task TCB (always kept ready at lowest priority).
 */
extern TCB_t *idle_tcb;

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
