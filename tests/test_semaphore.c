/**
 * @file test_semaphore.c
 * @brief Binary semaphore post/wait smoke test.
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

static semaphore_t g_sem;
volatile uint32_t g_semaphore_test_pass;

static void poster_task(void);
static void waiter_task(void);

int main(void)
{
    if (task_init() != RTOS_OK) {
        while (1) {
        }
    }

    g_semaphore_test_pass = 0u;
    (void)semaphore_init_binary(&g_sem, false);

    if (task_create(waiter_task, 32u, 768u, "wait") == NULL) {
        while (1) {
        }
    }

    if (task_create(poster_task, 64u, 768u, "post") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void poster_task(void)
{
    (void)task_delay(10u);
    (void)semaphore_post(&g_sem);
    while (1) {
        (void)task_delay(100u);
    }
}

static void waiter_task(void)
{
    if (semaphore_wait(&g_sem, 0u) == RTOS_OK) {
        g_semaphore_test_pass = 1u;
    }

    while (1) {
        (void)task_delay(100u);
    }
}
