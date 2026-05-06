/**
 * @file main.c
 * @brief Priority inheritance demonstration (classic inversion scenario).
 *
 * Scenario:
 * - Task L (low numeric bucket): locks mutex, holds critical section.
 * - Task H (high): blocked on mutex when L holds it; PI raises L's priority.
 * - Task M (medium): competes for CPU; without PI, could delay L and starve H.
 *
 * Smaller @c priority value = higher scheduling urgency in this kernel.
 * Use GDB: @c demo_phase, @c last_event, @c current_tcb->priority on Task L.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "rtos.h"
#include "scheduler.h"

static mutex_t g_shared_mutex;
static volatile uint32_t g_demo_phase;
static volatile char g_last_event[64];

/* Priorities: H=32 (bucket 1), M=96 (bucket 3), L=200 (bucket 6) */
static void task_low(void);
static void task_medium(void);
static void task_high(void);

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

    (void)mutex_init(&g_shared_mutex);
    g_demo_phase = 0u;
    g_last_event[0] = '\0';

    if (task_create(task_low, 200u, 1024u, "L") == NULL) {
        while (1) {
        }
    }

    if (task_create(task_medium, 96u, 1024u, "M") == NULL) {
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
        (void)mutex_lock(&g_shared_mutex, RTOS_WAIT_FOREVER);
        g_demo_phase = 1u;
        (void)snprintf((char *)g_last_event, sizeof(g_last_event),
                       "L: locked, pri=%u", (unsigned)current_tcb->priority);

        for (volatile uint32_t i = 0u; i < 50000u; i++) {
        }

        g_demo_phase = 2u;
        (void)snprintf((char *)g_last_event, sizeof(g_last_event),
                       "L: in CS pri=%u (expect boost if H blocked)",
                       (unsigned)current_tcb->priority);

        (void)mutex_unlock(&g_shared_mutex);
        g_demo_phase = 3u;
        (void)snprintf((char *)g_last_event, sizeof(g_last_event),
                       "L: unlocked pri=%u",
                       (unsigned)current_tcb->priority);

        (void)task_delay(100u);
    }
}

static void task_medium(void)
{
    while (1) {
        (void)snprintf((char *)g_last_event, sizeof(g_last_event),
                         "M: run pri=%u", (unsigned)current_tcb->priority);
        for (volatile uint32_t i = 0u; i < 10000u; i++) {
        }
        (void)task_delay(5u);
    }
}

static void task_high(void)
{
    /* Let L acquire mutex first */
    (void)task_delay(60u);

    while (1) {
        (void)snprintf((char *)g_last_event, sizeof(g_last_event),
                         "H: lock attempt pri=%u",
                         (unsigned)current_tcb->priority);
        (void)mutex_lock(&g_shared_mutex, RTOS_WAIT_FOREVER);
        (void)snprintf((char *)g_last_event, sizeof(g_last_event), "H: got lock");
        (void)mutex_unlock(&g_shared_mutex);
        (void)task_delay(100u);
    }
}
