/**
 * @file semaphore.c
 * @brief Counting/binary semaphore implementation with FIFO wait queue.
 */

#include "semaphore.h"

#include <stddef.h>
#include <string.h>

#include "scheduler.h"

extern volatile TCB_t *current_tcb;

static void _wait_enqueue(semaphore_t *sem, TCB_t *t)
{
    t->mutex_wait_next = NULL;
    if (sem->wait_head == NULL) {
        sem->wait_head = t;
        sem->wait_tail = t;
        return;
    }

    sem->wait_tail->mutex_wait_next = t;
    sem->wait_tail = t;
}

static TCB_t *_wait_pop_head(semaphore_t *sem)
{
    TCB_t *w;

    w = sem->wait_head;
    if (w == NULL) {
        return NULL;
    }

    sem->wait_head = w->mutex_wait_next;
    if (sem->wait_head == NULL) {
        sem->wait_tail = NULL;
    }

    w->mutex_wait_next = NULL;
    return w;
}

/**
 * @brief Initialize counting semaphore.
 *
 * @param sem Semaphore object.
 * @param initial Initial count.
 * @param max_count Maximum count value.
 * @return Status.
 */
rtos_status_t semaphore_init(semaphore_t *sem, uint32_t initial, uint32_t max_count)
{
    if ((sem == NULL) || (max_count == 0u) || (initial > max_count)) {
        return RTOS_ERR_PARAM;
    }

    memset(sem, 0, sizeof(*sem));
    sem->count = initial;
    sem->max_count = max_count;
    return RTOS_OK;
}

/**
 * @brief Initialize binary semaphore (0 or 1).
 *
 * @param sem Semaphore object.
 * @param initially_available True if starting available.
 * @return Status.
 */
rtos_status_t semaphore_init_binary(semaphore_t *sem, bool initially_available)
{
    uint32_t initial;

    initial = initially_available ? 1u : 0u;
    return semaphore_init(sem, initial, 1u);
}

/**
 * @brief Wait for semaphore (decrement). Blocks when count is zero.
 *
 * @param sem Semaphore object.
 * @param timeout_ticks Timeout in ticks (0 = infinite).
 * @return Status.
 */
rtos_status_t semaphore_wait(semaphore_t *sem, uint32_t timeout_ticks)
{
    TCB_t *self;

    if (sem == NULL) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    (void)timeout_ticks;

    for (;;) {
        __asm volatile ("cpsid i" ::: "memory");

        if (sem->count > 0u) {
            sem->count--;
            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        self->state = TASK_STATE_BLOCKED;
        scheduler_remove_task(self);
        _wait_enqueue(sem, self);

        __asm volatile ("cpsie i" ::: "memory");
        scheduler_mark_reschedule();

        while (self->state == TASK_STATE_BLOCKED) {
            __asm volatile ("nop");
        }
    }
}

/**
 * @brief Post semaphore (increment) up to max_count.
 *
 * @param sem Semaphore object.
 * @return Status.
 */
rtos_status_t semaphore_post(semaphore_t *sem)
{
    TCB_t *wake;

    if (sem == NULL) {
        return RTOS_ERR_PARAM;
    }

    __asm volatile ("cpsid i" ::: "memory");

    if (sem->count >= sem->max_count) {
        __asm volatile ("cpsie i" ::: "memory");
        return RTOS_ERR_FULL;
    }

    sem->count++;
    wake = _wait_pop_head(sem);
    if (wake != NULL) {
        wake->state = TASK_STATE_READY;
        (void)scheduler_add_task(wake);
    }

    __asm volatile ("cpsie i" ::: "memory");
    scheduler_mark_reschedule();
    return RTOS_OK;
}
