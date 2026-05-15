/**
 * @file semaphore.c
 * @brief Counting/binary semaphore implementation with FIFO wait queue.
 */

#include "semaphore.h"

#include <stddef.h>
#include <string.h>

#include "block.h"
#include "scheduler.h"

extern volatile TCB_t *current_tcb;

static void _wait_unlink(semaphore_t *sem, TCB_t *t)
{
    TCB_t *prev;
    TCB_t *walk;

    if ((sem == NULL) || (t == NULL) || (sem->wait_head == NULL)) {
        return;
    }

    prev = NULL;
    walk = sem->wait_head;
    while (walk != NULL) {
        if (walk == t) {
            if (prev == NULL) {
                sem->wait_head = t->mutex_wait_next;
            }
            else {
                prev->mutex_wait_next = t->mutex_wait_next;
            }
            if (t == sem->wait_tail) {
                sem->wait_tail = prev;
            }
            if (sem->wait_head == NULL) {
                sem->wait_tail = NULL;
            }
            t->mutex_wait_next = NULL;
            return;
        }
        prev = walk;
        walk = walk->mutex_wait_next;
    }
}

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

void semaphore_wake_timeout(TCB_t *t)
{
    semaphore_t *sem;

    if (t == NULL) {
        return;
    }

    sem = (semaphore_t *)t->block_object;
    if (sem != NULL) {
        _wait_unlink(sem, t);
    }

    t->block_reason = BLOCK_NONE;
    t->block_object = NULL;
    t->block_wake_status = RTOS_ERR_TIMEOUT;
    t->state = TASK_STATE_READY;
    (void)scheduler_add_task(t);
}

void semaphore_detach_waiter(semaphore_t *sem, TCB_t *t)
{
    if ((sem == NULL) || (t == NULL)) {
        return;
    }

    _wait_unlink(sem, t);
    t->mutex_wait_next = NULL;
}

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

rtos_status_t semaphore_init_binary(semaphore_t *sem, bool initially_available)
{
    uint32_t initial;

    initial = initially_available ? 1u : 0u;
    return semaphore_init(sem, initial, 1u);
}

rtos_status_t semaphore_wait(semaphore_t *sem, uint32_t timeout_ticks)
{
    TCB_t *self;
    rtos_status_t st;

    if (sem == NULL) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    for (;;) {
        __asm volatile ("cpsid i" ::: "memory");

        if (sem->count > 0u) {
            sem->count--;
            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        _wait_enqueue(sem, self);

        __asm volatile ("cpsie i" ::: "memory");

        st = rtos_block_current(timeout_ticks, BLOCK_SEMAPHORE, sem);
        if (st == RTOS_ERR_TIMEOUT) {
            return RTOS_ERR_TIMEOUT;
        }
    }
}

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
        wake->block_reason = BLOCK_NONE;
        wake->block_object = NULL;
        wake->state = TASK_STATE_READY;
        (void)scheduler_add_task(wake);
    }

    __asm volatile ("cpsie i" ::: "memory");
    scheduler_mark_reschedule();
    return RTOS_OK;
}
