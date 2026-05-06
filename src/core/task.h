/**
 * @file task.h
 * @brief Task management API.
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize kernel task subsystem (calls scheduler + idle creation).
 *
 * @return Status.
 */
rtos_status_t task_init(void);

/**
 * @brief Create a new task.
 *
 * @param task_func Pointer to task entry function.
 * @param priority Task priority (0 = highest logical per config mapping).
 * @param stack_size Stack size in bytes (minimum @ref RTOS_MIN_STACK_SIZE).
 * @param name Task name (max @ref RTOS_TASK_NAME_MAX including null).
 * @return Pointer to TCB, or NULL on failure.
 */
TCB_t *task_create(void (*task_func)(void), uint8_t priority, uint32_t stack_size,
                   const char *name);

/**
 * @brief Delete a task (caller must not delete currently running task from itself).
 *
 * @param tcb Task control block.
 * @return Status.
 */
rtos_status_t task_delete(TCB_t *tcb);

/**
 * @brief Yield CPU to the scheduler.
 */
void task_yield(void);

/**
 * @brief Delay current task for a number of ticks.
 *
 * @param ticks Number of system ticks (must be > 0).
 * @return Status.
 */
rtos_status_t task_delay(uint32_t ticks);

/**
 * @brief Enter multitasking: configure SysTick and perform first dispatch.
 *
 * @return Does not return if successful.
 */
rtos_status_t rtos_start(void);

/**
 * @brief Number of elapsed system ticks (1 ms default).
 *
 * @return Tick count (wraps uint32_t).
 */
uint32_t rtos_get_tick(void);

extern volatile const char *g_rtos_stack_overflow_task_name;
extern volatile uint32_t g_rtos_stack_overflow_canary;

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
