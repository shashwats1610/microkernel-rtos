/**
 * @file block.h
 * @brief Shared blocking and timeout helpers for tasks.
 */

#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Block current task until ready; honor timeout via SysTick wake.
 *
 * @param timeout_ticks Ticks to wait (0 = @ref RTOS_WAIT_FOREVER).
 * @param reason Block reason for timeout dispatcher.
 * @param obj Related object (mutex, semaphore, or message queue).
 * @return @ref RTOS_OK on normal wake, @ref RTOS_ERR_TIMEOUT on timeout.
 */
rtos_status_t rtos_block_current(uint32_t timeout_ticks, block_reason_t reason, void *obj);

#ifdef __cplusplus
}
#endif

#endif /* BLOCK_H */
