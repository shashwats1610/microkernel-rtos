/**
 * @file test_mutex.c
 * @brief Mutex lock/unlock smoke path from multiple priorities.
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

static mutex_t g_mtx;
volatile uint32_t g_mutex_test_pass;

static void holder_task(void);
static void contender_task(void);

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

    g_mutex_test_pass = 0u;
    (void)mutex_init(&g_mtx);

    if (task_create(holder_task, 64u, 768u, "holder") == NULL) {
        while (1) {
        }
    }

    if (task_create(contender_task, 32u, 768u, "cont") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void holder_task(void)
{
    (void)mutex_lock(&g_mtx, 0u);
    g_mutex_test_pass = 1u;
    (void)task_delay(20u);
    (void)mutex_unlock(&g_mtx);

    while (1) {
        (void)task_delay(50u);
    }
}

static void contender_task(void)
{
    while (1) {
        (void)mutex_lock(&g_mtx, 0u);
        (void)mutex_unlock(&g_mtx);
        (void)task_delay(10u);
    }
}
