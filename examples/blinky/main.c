/**
 * @file main.c
 * @brief Minimal multitasking sanity check (delay/yield path).
 */

#include <stddef.h>

#include "rtos.h"

static void worker(void);

/**
 * @brief Application entry called from startup after hardware reset.
 *
 * @return Never.
 */
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
