/**
 * @file scheduler.c
 * @brief Priority queues with round-robin within each level.
 */

#include "scheduler.h"

#include <stddef.h>
#include <string.h>

#include "rtos_config.h"

volatile TCB_t *current_tcb;
TCB_t *idle_tcb;

static TCB_t *ready_head[RTOS_NUM_PRIORITIES];

static void _ring_insert(uint8_t bucket, TCB_t *tcb);

/**
 * @brief Initialize priority queues and idle bookkeeping.
 */
void scheduler_init(void)
{
    memset(ready_head, 0, sizeof(ready_head));
    current_tcb = NULL;
    idle_tcb = NULL;
}

/**
 * @brief Add a task to its priority ready ring.
 */
rtos_status_t scheduler_add_task(TCB_t *tcb)
{
    uint8_t bucket;

    if (tcb == NULL) {
        return RTOS_ERR_PARAM;
    }
    if (tcb->state == TASK_STATE_TERMINATED) {
        return RTOS_ERR_STATE;
    }

    bucket = tcb->prio_bucket;
    _ring_insert(bucket, tcb);
    if (tcb->state != TASK_STATE_BLOCKED && tcb->state != TASK_STATE_SUSPENDED) {
        tcb->state = TASK_STATE_READY;
    }
    return RTOS_OK;
}

static void _ring_insert(uint8_t bucket, TCB_t *tcb)
{
    TCB_t *head;

    head = ready_head[bucket];
    if (head == NULL) {
        tcb->next_ready = tcb;
        ready_head[bucket] = tcb;
        return;
    }

    {
        TCB_t *tail;

        tail = head;
        while (tail->next_ready != head) {
            tail = tail->next_ready;
        }
        tail->next_ready = tcb;
        tcb->next_ready = head;
    }
}

/**
 * @brief Remove a task from the ready ring if present.
 */
void scheduler_remove_task(TCB_t *tcb)
{
    uint8_t bucket;
    TCB_t *head;
    TCB_t *prev;

    if (tcb == NULL) {
        return;
    }

    bucket = tcb->prio_bucket;
    head = ready_head[bucket];
    if (head == NULL) {
        return;
    }

    prev = NULL;
    {
        TCB_t *walk;

        walk = head;
        while (1) {
            if (walk == tcb) {
                break;
            }
            prev = walk;
            walk = walk->next_ready;
            if (walk == head) {
                return;
            }
        }
    }

    if (tcb->next_ready == tcb) {
        ready_head[bucket] = NULL;
    }
    else {
        TCB_t *next_link;

        next_link = tcb->next_ready;
        if (prev == NULL) {
            TCB_t *tail;

            tail = head;
            while (tail->next_ready != head) {
                tail = tail->next_ready;
            }
            tail->next_ready = next_link;
            ready_head[bucket] = next_link;
        }
        else {
            prev->next_ready = next_link;
        }
    }
    tcb->next_ready = NULL;
}

/**
 * @brief Move a READY task between priority buckets after priority change.
 */
void scheduler_rebucket_task(TCB_t *tcb, uint8_t old_bucket, uint8_t new_bucket)
{
    if ((tcb == NULL) || (old_bucket == new_bucket)) {
        return;
    }

    if (tcb->state == TASK_STATE_READY) {
        tcb->prio_bucket = old_bucket;
        scheduler_remove_task(tcb);
        tcb->prio_bucket = new_bucket;
        (void)scheduler_add_task(tcb);
    }
    else {
        tcb->prio_bucket = new_bucket;
    }
}

/**
 * @brief Select next task for execution (called from PendSV only).
 */
TCB_t *scheduler_get_next(void)
{
    uint32_t p;
    TCB_t *prev;

    prev = (TCB_t *)current_tcb;

    if ((prev != NULL) && (prev->state == TASK_STATE_RUNNING)) {
        prev->state = TASK_STATE_READY;
    }

    for (p = 0; p < RTOS_NUM_PRIORITIES; p++) {
        if (ready_head[p] != NULL) {
            TCB_t *sel;

            sel = ready_head[p];
            ready_head[p] = sel->next_ready;
            sel->state = TASK_STATE_RUNNING;
            sel->time_slice_counter = 0u;
            current_tcb = sel;
            return sel;
        }
    }

    if (idle_tcb != NULL) {
        idle_tcb->state = TASK_STATE_RUNNING;
        idle_tcb->time_slice_counter = 0u;
        current_tcb = idle_tcb;
        return idle_tcb;
    }

    while (1) {
    }
}

/**
 * @brief Request reschedule via PendSV.
 */
void scheduler_mark_reschedule(void)
{
    volatile uint32_t *icsr;

    icsr = (volatile uint32_t *)0xE000ED04UL;
    *icsr = 0x10000000UL;
}

/**
 * @brief Advance time-slice counter; preempt if slice expired and peers exist.
 */
void scheduler_time_slice_tick(void)
{
    TCB_t *cur;
    uint8_t bucket;
    TCB_t *head;

    cur = (TCB_t *)current_tcb;
    if ((cur == NULL) || (cur == idle_tcb) || (cur->state != TASK_STATE_RUNNING)) {
        return;
    }

    cur->time_slice_counter++;
    if (cur->time_slice_counter < RTOS_TIME_SLICE_TICKS) {
        return;
    }

    bucket = cur->prio_bucket;
    head = ready_head[bucket];
    if ((head == NULL) || (head->next_ready == head)) {
        cur->time_slice_counter = 0u;
        return;
    }

    cur->time_slice_counter = 0u;
    scheduler_mark_reschedule();
}
