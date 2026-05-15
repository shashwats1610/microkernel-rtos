/**
 * @file block.c
 * @brief Task blocking with optional timeout.
 */

#include "block.h"

#include <stddef.h>
#include <stdint.h>

#include "rtos_config.h"
#include "scheduler.h"
#include "task.h"

extern volatile TCB_t *current_tcb;

/**
 * @brief Block current task until ready; honor timeout via SysTick wake.
 */
rtos_status_t rtos_block_current(uint32_t timeout_ticks, block_reason_t reason, void *obj)
{
    TCB_t *self;

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    self->block_reason = reason;
    self->block_object = obj;
    self->block_wake_status = RTOS_OK;

    __asm volatile ("cpsid i" ::: "memory");

    if ((timeout_ticks == RTOS_WAIT_FOREVER) || (timeout_ticks == 0u)) {
        self->wake_time = UINT32_MAX;
    }
    else {
        self->wake_time = rtos_get_tick() + timeout_ticks;
    }

    self->state = TASK_STATE_BLOCKED;
    scheduler_remove_task(self);

    __asm volatile ("cpsie i" ::: "memory");
    scheduler_mark_reschedule();

    while (self->state == TASK_STATE_BLOCKED) {
        __asm volatile ("nop");
    }

    return self->block_wake_status;
}
