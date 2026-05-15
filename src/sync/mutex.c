/**
 * @file mutex.c
 * @brief Mutex with priority inheritance and priority-ordered wait queue handoff.
 */

#include "mutex.h"

#include <stddef.h>
#include <string.h>

#include "block.h"
#include "rtos_config.h"
#include "scheduler.h"

extern volatile TCB_t *current_tcb;

static uint8_t _min_wait_priority_value(const mutex_t *m)
{
    uint8_t min_pri;
    TCB_t *w;

    w = m->wait_head;
    if (w == NULL) {
        return RTOS_PRIORITY_MAX;
    }

    min_pri = w->priority;
    w = w->mutex_wait_next;
    while (w != NULL) {
        if (w->priority < min_pri) {
            min_pri = w->priority;
        }
        w = w->mutex_wait_next;
    }
    return min_pri;
}

static void _apply_pi(mutex_t *m)
{
#if RTOS_MUTEX_PRIORITY_INHERITANCE
    TCB_t *owner;
    uint8_t old_bucket;
    uint8_t new_bucket;

    owner = m->owner;
    if ((owner == NULL) || (m->wait_head == NULL)) {
        return;
    }

    {
        uint8_t hp;

        hp = _min_wait_priority_value(m);
        if (hp < owner->priority) {
            old_bucket = owner->prio_bucket;
            owner->priority = hp;
            owner->prio_bucket = (uint8_t)(hp >> 5);
            if (owner->prio_bucket >= RTOS_NUM_PRIORITIES) {
                owner->prio_bucket = RTOS_NUM_PRIORITIES - 1u;
            }
            new_bucket = owner->prio_bucket;
            scheduler_rebucket_task(owner, old_bucket, new_bucket);
        }
    }
#else
    (void)m;
#endif
}

static void _restore_owner_priority(TCB_t *tcb)
{
    uint8_t old_bucket;
    uint8_t new_bucket;

    if (tcb == NULL) {
        return;
    }

    old_bucket = tcb->prio_bucket;
    tcb->priority = tcb->base_priority;
    tcb->prio_bucket = (uint8_t)(tcb->base_priority >> 5);
    if (tcb->prio_bucket >= RTOS_NUM_PRIORITIES) {
        tcb->prio_bucket = RTOS_NUM_PRIORITIES - 1u;
    }
    new_bucket = tcb->prio_bucket;
    scheduler_rebucket_task(tcb, old_bucket, new_bucket);
}

static void _wait_unlink(mutex_t *m, TCB_t *t)
{
    TCB_t *prev;
    TCB_t *walk;

    if ((m == NULL) || (t == NULL) || (m->wait_head == NULL)) {
        return;
    }

    prev = NULL;
    walk = m->wait_head;
    while (walk != NULL) {
        if (walk == t) {
            if (prev == NULL) {
                m->wait_head = t->mutex_wait_next;
            }
            else {
                prev->mutex_wait_next = t->mutex_wait_next;
            }
            if (t == m->wait_tail) {
                m->wait_tail = prev;
            }
            if (m->wait_head == NULL) {
                m->wait_tail = NULL;
            }
            t->mutex_wait_next = NULL;
            return;
        }
        prev = walk;
        walk = walk->mutex_wait_next;
    }
}

static void _wait_enqueue_tail(mutex_t *m, TCB_t *t)
{
    t->mutex_wait_next = NULL;
    if (m->wait_head == NULL) {
        m->wait_head = t;
        m->wait_tail = t;
        return;
    }

    m->wait_tail->mutex_wait_next = t;
    m->wait_tail = t;
}

static TCB_t *_wait_pop_highest(mutex_t *m)
{
    TCB_t *best;
    TCB_t *best_prev;
    TCB_t *prev;
    TCB_t *walk;

    if (m->wait_head == NULL) {
        return NULL;
    }

    best = m->wait_head;
    best_prev = NULL;
    prev = NULL;
    walk = m->wait_head;
    while (walk != NULL) {
        if (walk->priority < best->priority) {
            best = walk;
            best_prev = prev;
        }
        prev = walk;
        walk = walk->mutex_wait_next;
    }

    if (best_prev == NULL) {
        m->wait_head = best->mutex_wait_next;
        if (m->wait_head == NULL) {
            m->wait_tail = NULL;
        }
    }
    else {
        best_prev->mutex_wait_next = best->mutex_wait_next;
        if (best == m->wait_tail) {
            m->wait_tail = best_prev;
        }
    }

    best->mutex_wait_next = NULL;
    return best;
}

void mutex_wake_timeout(TCB_t *t)
{
    mutex_t *m;

    if (t == NULL) {
        return;
    }

    m = (mutex_t *)t->block_object;
    if (m != NULL) {
        _wait_unlink(m, t);
    }

    t->blocked_on_mutex = NULL;
    t->block_reason = BLOCK_NONE;
    t->block_object = NULL;
    t->block_wake_status = RTOS_ERR_TIMEOUT;
    t->state = TASK_STATE_READY;
    (void)scheduler_add_task(t);
}

void mutex_detach_waiter(mutex_t *m, TCB_t *t)
{
    if ((m == NULL) || (t == NULL)) {
        return;
    }

    _wait_unlink(m, t);
    t->blocked_on_mutex = NULL;
    t->mutex_wait_next = NULL;
}

rtos_status_t mutex_init(mutex_t *m)
{
    if (m == NULL) {
        return RTOS_ERR_PARAM;
    }

    memset(m, 0, sizeof(*m));
    return RTOS_OK;
}

rtos_status_t mutex_lock(mutex_t *m, uint32_t timeout_ticks)
{
    TCB_t *self;
    rtos_status_t st;

    if (m == NULL) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    for (;;) {
        __asm volatile ("cpsid i" ::: "memory");

        if (m->locked && (m->owner == self)) {
            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        if (!m->locked) {
            m->locked = true;
            m->owner = self;
            self->blocked_on_mutex = NULL;
            __asm volatile ("cpsie i" ::: "memory");
            return RTOS_OK;
        }

        self->blocked_on_mutex = m;
        _wait_enqueue_tail(m, self);
        _apply_pi(m);

        __asm volatile ("cpsie i" ::: "memory");

        st = rtos_block_current(timeout_ticks, BLOCK_MUTEX, m);
        if (st == RTOS_ERR_TIMEOUT) {
            return RTOS_ERR_TIMEOUT;
        }
    }
}

rtos_status_t mutex_unlock(mutex_t *m)
{
    TCB_t *self;
    TCB_t *next;

    if (m == NULL) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if ((self == NULL) || !m->locked || (m->owner != self)) {
        return RTOS_ERR_OWNER;
    }

    __asm volatile ("cpsid i" ::: "memory");

    _restore_owner_priority(self);
    next = _wait_pop_highest(m);

    if (next == NULL) {
        m->locked = false;
        m->owner = NULL;
    }
    else {
        m->owner = next;
        m->locked = true;
        next->blocked_on_mutex = NULL;
        next->block_reason = BLOCK_NONE;
        next->block_object = NULL;
        next->state = TASK_STATE_READY;
        (void)scheduler_add_task(next);
    }

    __asm volatile ("cpsie i" ::: "memory");
    scheduler_mark_reschedule();
    return RTOS_OK;
}
