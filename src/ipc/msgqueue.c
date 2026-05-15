/**
 * @file msgqueue.c
 * @brief FIFO queue with scheduler blocking and timeouts.
 */

#include "msgqueue.h"

#include <stddef.h>
#include <string.h>

#include "block.h"
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

static void _tcb_unlink(TCB_t **head, TCB_t **tail, TCB_t *t)
{
    TCB_t *prev;
    TCB_t *walk;

    if ((head == NULL) || (*head == NULL) || (t == NULL)) {
        return;
    }

    prev = NULL;
    walk = *head;
    while (walk != NULL) {
        if (walk == t) {
            if (prev == NULL) {
                *head = t->mutex_wait_next;
            }
            else {
                prev->mutex_wait_next = t->mutex_wait_next;
            }
            if (t == *tail) {
                *tail = prev;
            }
            if (*head == NULL) {
                *tail = NULL;
            }
            t->mutex_wait_next = NULL;
            return;
        }
        prev = walk;
        walk = walk->mutex_wait_next;
    }
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
        t->block_reason = BLOCK_NONE;
        t->block_object = NULL;
        t->state = TASK_STATE_READY;
        (void)scheduler_add_task(t);
        scheduler_mark_reschedule();
    }
}

void msgqueue_wake_timeout(TCB_t *t)
{
    msg_queue_t *q;

    if (t == NULL) {
        return;
    }

    q = (msg_queue_t *)t->block_object;
    if (q != NULL) {
        if (t->block_reason == BLOCK_MSG_SEND) {
            _tcb_unlink(&q->send_wait_head, &q->send_wait_tail, t);
        }
        else if (t->block_reason == BLOCK_MSG_RECV) {
            _tcb_unlink(&q->recv_wait_head, &q->recv_wait_tail, t);
        }
    }

    t->block_reason = BLOCK_NONE;
    t->block_object = NULL;
    t->block_wake_status = RTOS_ERR_TIMEOUT;
    t->state = TASK_STATE_READY;
    (void)scheduler_add_task(t);
}

void msgqueue_detach_waiter(msg_queue_t *q, TCB_t *t)
{
    if ((q == NULL) || (t == NULL)) {
        return;
    }

    if (t->block_reason == BLOCK_MSG_SEND) {
        _tcb_unlink(&q->send_wait_head, &q->send_wait_tail, t);
    }
    else if (t->block_reason == BLOCK_MSG_RECV) {
        _tcb_unlink(&q->recv_wait_head, &q->recv_wait_tail, t);
    }

    t->mutex_wait_next = NULL;
}

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

rtos_status_t msg_queue_send(msg_queue_t *q, const void *msg, uint32_t timeout_ticks)
{
    TCB_t *self;
    rtos_status_t st;

    if ((q == NULL) || (msg == NULL)) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

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

        _tcb_enqueue_tail(&q->send_wait_head, &q->send_wait_tail, self);

        __asm volatile ("cpsie i" ::: "memory");

        st = rtos_block_current(timeout_ticks, BLOCK_MSG_SEND, q);
        if (st == RTOS_ERR_TIMEOUT) {
            return RTOS_ERR_TIMEOUT;
        }
    }
}

rtos_status_t msg_queue_recv(msg_queue_t *q, void *out, uint32_t timeout_ticks)
{
    TCB_t *self;
    rtos_status_t st;

    if ((q == NULL) || (out == NULL)) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

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

        _tcb_enqueue_tail(&q->recv_wait_head, &q->recv_wait_tail, self);

        __asm volatile ("cpsie i" ::: "memory");

        st = rtos_block_current(timeout_ticks, BLOCK_MSG_RECV, q);
        if (st == RTOS_ERR_TIMEOUT) {
            return RTOS_ERR_TIMEOUT;
        }
    }
}
