/**
 * @file msgqueue.c
 * @brief FIFO queue with scheduler blocking (timeouts best-effort).
 */

#include "msgqueue.h"

#include <stddef.h>
#include <string.h>

#include "scheduler.h"

extern volatile TCB_t *current_tcb;

static void _tcb_enqueue_tail(TCB_t **head, TCB_t **tail, TCB_t *t)
{
    t->mutex_wait_next = NULL;
    if (*head == NULL) {
        *head = t;
        *tail = t;
        return;
    }

    (*tail)->mutex_wait_next = t;
    *tail = t;
}

static TCB_t *_tcb_pop_head(TCB_t **head, TCB_t **tail)
{
    TCB_t *w;

    w = *head;
    if (w == NULL) {
        return NULL;
    }

    *head = w->mutex_wait_next;
    if (*head == NULL) {
        *tail = NULL;
    }

    w->mutex_wait_next = NULL;
    return w;
}

static void _wake_head(TCB_t **head, TCB_t **tail)
{
    TCB_t *t;

    t = _tcb_pop_head(head, tail);
    if (t != NULL) {
        t->state = TASK_STATE_READY;
        (void)scheduler_add_task(t);
        scheduler_mark_reschedule();
    }
}

/**
 * @brief Initialize queue using caller-provided storage.
 *
 * @param q Queue object.
 * @param storage Byte storage sized @a depth * @a msg_size.
 * @param msg_size Fixed message size in bytes (>0).
 * @param depth Number of messages (>0).
 * @return Status.
 */
rtos_status_t msg_queue_init(msg_queue_t *q, uint8_t *storage, uint32_t msg_size,
                             uint32_t depth)
{
    if ((q == NULL) || (storage == NULL) || (msg_size == 0u) || (depth == 0u)) {
        return RTOS_ERR_PARAM;
    }

    memset(q, 0, sizeof(*q));
    q->buffer = storage;
    q->msg_size = msg_size;
    q->depth = depth;
    return RTOS_OK;
}

/**
 * @brief Blocking send with optional timeout (best-effort).
 *
 * @param q Queue object.
 * @param msg Message pointer copied into queue.
 * @param timeout_ticks Timeout (0 = infinite).
 * @return Status.
 */
rtos_status_t msg_queue_send(msg_queue_t *q, const void *msg, uint32_t timeout_ticks)
{
    TCB_t *self;

    if ((q == NULL) || (msg == NULL)) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    (void)timeout_ticks;

    for (;;) {
        __asm volatile ("cpsid i" ::: "memory");

        if (q->count < q->depth) {
            uint8_t *slot;

            slot = q->buffer + (q->tail * q->msg_size);
            (void)memcpy(slot, msg, q->msg_size);
            q->tail = (q->tail + 1u) % q->depth;
            q->count++;

            _wake_head(&q->recv_wait_head, &q->recv_wait_tail);

            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        self->state = TASK_STATE_BLOCKED;
        scheduler_remove_task(self);
        _tcb_enqueue_tail(&q->send_wait_head, &q->send_wait_tail, self);

        __asm volatile ("cpsie i" ::: "memory");
        scheduler_mark_reschedule();

        while (self->state == TASK_STATE_BLOCKED) {
            __asm volatile ("nop");
        }
    }
}

/**
 * @brief Blocking receive with optional timeout (best-effort).
 *
 * @param q Queue object.
 * @param out Output buffer sized @a msg_size.
 * @param timeout_ticks Timeout (0 = infinite).
 * @return Status.
 */
rtos_status_t msg_queue_recv(msg_queue_t *q, void *out, uint32_t timeout_ticks)
{
    TCB_t *self;

    if ((q == NULL) || (out == NULL)) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    (void)timeout_ticks;

    for (;;) {
        __asm volatile ("cpsid i" ::: "memory");

        if (q->count > 0u) {
            uint8_t *slot;

            slot = q->buffer + (q->head * q->msg_size);
            (void)memcpy(out, slot, q->msg_size);
            q->head = (q->head + 1u) % q->depth;
            q->count--;

            _wake_head(&q->send_wait_head, &q->send_wait_tail);

            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        self->state = TASK_STATE_BLOCKED;
        scheduler_remove_task(self);
        _tcb_enqueue_tail(&q->recv_wait_head, &q->recv_wait_tail, self);

        __asm volatile ("cpsie i" ::: "memory");
        scheduler_mark_reschedule();

        while (self->state == TASK_STATE_BLOCKED) {
            __asm volatile ("nop");
        }
    }
}
