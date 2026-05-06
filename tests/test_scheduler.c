/**
 * @file test_scheduler.c
 * @brief Scheduler sanity: delay advances tick counter as observed by a task.
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

volatile uint32_t g_scheduler_test_pass;

static void tick_test_task(void);

/**
 * @brief Standalone test entry (linked as the application main).
 *
 * @return Never.
 */
int main(void)
{
    if (task_init() != RTOS_OK) {
        while (1) {
        }
    }

    g_scheduler_test_pass = 0u;
    if (task_create(tick_test_task, 16u, 768u, "tick_test") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void tick_test_task(void)
{
    uint32_t a;
    uint32_t b;

    a = rtos_get_tick();
    (void)task_delay(5u);
    b = rtos_get_tick();

    if ((b - a) >= 5u) {
        g_scheduler_test_pass = 1u;
    }

    while (1) {
        (void)task_delay(100u);
    }
}
