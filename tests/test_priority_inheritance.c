/**
 * @file test_priority_inheritance.c
 * @brief Classic Mars Pathfinder-style inversion scenario (best-effort demonstration).
 *
 * Task L locks mutex; Task H blocks on mutex; Task M runs CPU-bound.
 * With PI enabled (see RTOS_MUTEX_PRIORITY_INHERITANCE), L should execute sooner.
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

static mutex_t g_mtx;

volatile uint32_t g_pi_high_enter_tick;
volatile uint32_t g_pi_low_hold_tick;
volatile uint32_t g_pi_test_note;

static void task_low(void);
static void task_mid(void);
static void task_high(void);

/**
 * @brief Standalone test entry.
 *
 * @return Never.
 */
int main(void)
{
    if (task_init() != RTOS_OK) {
        while (1) {
        }
    }

    g_pi_high_enter_tick = 0u;
    g_pi_low_hold_tick = 0u;
    g_pi_test_note = 0u;

    (void)mutex_init(&g_mtx);

    if (task_create(task_low, 224u, 1024u, "L") == NULL) {
        while (1) {
        }
    }

    if (task_create(task_mid, 96u, 1024u, "M") == NULL) {
        while (1) {
        }
    }

    if (task_create(task_high, 32u, 1024u, "H") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void task_low(void)
{
    while (1) {
        (void)mutex_lock(&g_mtx, 0u);
        g_pi_low_hold_tick = rtos_get_tick();
        (void)task_delay(50u);
        (void)mutex_unlock(&g_mtx);
        (void)task_delay(200u);
    }
}

static void task_mid(void)
{
    while (1) {
        volatile uint32_t spin;

        for (spin = 0u; spin < 80000u; spin++) {
            __asm volatile ("nop");
        }
        (void)task_delay(1u);
    }
}

static void task_high(void)
{
    while (1) {
        (void)task_delay(15u);
        g_pi_high_enter_tick = rtos_get_tick();
        (void)mutex_lock(&g_mtx, 0u);
        g_pi_test_note = 1u;
        (void)mutex_unlock(&g_mtx);
        (void)task_delay(50u);
    }
}
