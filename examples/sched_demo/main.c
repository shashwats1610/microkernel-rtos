/**
 * @file main.c
 * @brief Scheduling demo: task_delay and task_yield (no GPIO).
 */

#include <stddef.h>

#include "rtos.h"

static void worker(void);

int main(void)
{
    if (task_init() != RTOS_OK) {
        while (1) {
        }
    }

    if (task_create(worker, 32u, 512u, "worker") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void worker(void)
{
    while (1) {
        (void)task_delay(100u);
        task_yield();
    }
}
