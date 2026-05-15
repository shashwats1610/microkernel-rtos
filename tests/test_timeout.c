/**
 * @file test_timeout.c
 * @brief Mutex lock timeout returns RTOS_ERR_TIMEOUT.
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

static mutex_t g_mtx;
volatile uint32_t g_timeout_test_pass;

static void holder_task(void);
static void waiter_task(void);

int main(void)
{
    if (task_init() != RTOS_OK) {
        while (1) {
        }
    }

    g_timeout_test_pass = 0u;
    (void)mutex_init(&g_mtx);

    if (task_create(holder_task, 64u, 768u, "hold") == NULL) {
        while (1) {
        }
    }

    if (task_create(waiter_task, 32u, 768u, "wait") == NULL) {
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
    while (1) {
        (void)task_delay(50u);
    }
}

static void waiter_task(void)
{
    (void)task_delay(5u);
    if (mutex_lock(&g_mtx, 8u) == RTOS_ERR_TIMEOUT) {
        g_timeout_test_pass = 1u;
    }

    while (1) {
        (void)task_delay(100u);
    }
}
