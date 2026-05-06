/**
 * @file sw_timer.c
 * @brief Sorted singly linked list of software timers (insertion sort on arm).
 */

#include "sw_timer.h"

#include <stddef.h>
#include <string.h>

#include "rtos_config.h"

extern uint32_t rtos_get_tick(void);

static sw_timer_t *timer_list;

/**
 * @brief Initialize global timer list (call once).
 */
void sw_timer_init(void)
{
    timer_list = NULL;
}

static void _insert_sorted(sw_timer_t *t)
{
    sw_timer_t *curr;
    sw_timer_t *prev;

    prev = NULL;
    curr = timer_list;
    while ((curr != NULL) && (curr->expiry_tick <= t->expiry_tick)) {
        prev = curr;
        curr = curr->next;
    }

    if (prev == NULL) {
        t->next = timer_list;
        timer_list = t;
    }
    else {
        t->next = curr;
        prev->next = t;
    }
}

/**
 * @brief Arm a one-shot timer.
 *
 * @param t Timer object (caller storage).
 * @param delay_ticks Delay from now.
 * @param cb Callback (keep minimal; no blocking APIs).
 * @param arg User argument passed to callback.
 * @return Status.
 */
rtos_status_t sw_timer_start_once(sw_timer_t *t, uint32_t delay_ticks,
                                  sw_timer_callback_t cb, void *arg)
{
    if ((t == NULL) || (cb == NULL) || (delay_ticks == 0u)) {
        return RTOS_ERR_PARAM;
    }

    memset(t, 0, sizeof(*t));
    t->expiry_tick = rtos_get_tick() + delay_ticks;
    t->period_ticks = 0u;
    t->callback = cb;
    t->callback_arg = arg;
    t->periodic = false;
    t->active = true;

    _insert_sorted(t);
    return RTOS_OK;
}

/**
 * @brief Arm a periodic timer.
 *
 * @param t Timer object.
 * @param period_ticks Period in ticks.
 * @param cb Callback.
 * @param arg User argument.
 * @return Status.
 */
rtos_status_t sw_timer_start_periodic(sw_timer_t *t, uint32_t period_ticks,
                                      sw_timer_callback_t cb, void *arg)
{
    if ((t == NULL) || (cb == NULL) || (period_ticks == 0u)) {
        return RTOS_ERR_PARAM;
    }

    memset(t, 0, sizeof(*t));
    t->expiry_tick = rtos_get_tick() + period_ticks;
    t->period_ticks = period_ticks;
    t->callback = cb;
    t->callback_arg = arg;
    t->periodic = true;
    t->active = true;

    _insert_sorted(t);
    return RTOS_OK;
}

/**
 * @brief Stop a timer if active.
 *
 * @param t Timer object.
 */
void sw_timer_stop(sw_timer_t *t)
{
    sw_timer_t *curr;
    sw_timer_t *prev;

    if (t == NULL) {
        return;
    }

    prev = NULL;
    curr = timer_list;
    while (curr != NULL) {
        if (curr == t) {
            if (prev == NULL) {
                timer_list = curr->next;
            }
            else {
                prev->next = curr->next;
            }
            t->next = NULL;
            t->active = false;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/**
 * @brief Expiry processing from SysTick (kernel-only).
 */
void sw_timer_tick_process(void)
{
    uint32_t now;

    now = rtos_get_tick();
    while ((timer_list != NULL) && (timer_list->active) &&
           (timer_list->expiry_tick <= now)) {
        sw_timer_t *due;

        due = timer_list;
        timer_list = due->next;
        due->next = NULL;

        if (due->callback != NULL) {
            due->callback(due->callback_arg);
        }

        if (due->periodic) {
            due->expiry_tick = now + due->period_ticks;
            due->next = NULL;
            _insert_sorted(due);
        }
        else {
            due->active = false;
        }
    }
}
