/**
 * @file sw_timer.h
 * @brief Software timers sorted by expiry (processed from SysTick context).
 */

#ifndef SW_TIMER_H
#define SW_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sw_timer_callback_t)(void *arg);

typedef struct sw_timer_s {
    uint32_t expiry_tick;
    uint32_t period_ticks;
    sw_timer_callback_t callback;
    void *callback_arg;
    bool periodic;
    bool active;
    struct sw_timer_s *next;
} sw_timer_t;

/**
 * @brief Initialize global timer list (call once).
 */
void sw_timer_init(void);

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
                                  sw_timer_callback_t cb, void *arg);

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
                                      sw_timer_callback_t cb, void *arg);

/**
 * @brief Stop a timer if active.
 *
 * @param t Timer object.
 */
void sw_timer_stop(sw_timer_t *t);

/**
 * @brief Expiry processing from SysTick (kernel-only).
 */
void sw_timer_tick_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SW_TIMER_H */
