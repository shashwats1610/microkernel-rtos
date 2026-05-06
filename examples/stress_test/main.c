/**
 * @file main.c
 * @brief Ten tasks with mixed priorities contending on one mutex.
 *
 * After long runs, inspect @c g_mutex_acquisition_count in GDB (all entries
 * should advance if the system remains fair and live).
 */

#include <stddef.h>
#include <stdint.h>

#include "rtos.h"

static mutex_t g_contended_mutex;
volatile uint32_t g_mutex_acquisition_count[10];

static void stress_body(uint32_t task_id);
static void stress_t0(void);
static void stress_t1(void);
static void stress_t2(void);
static void stress_t3(void);
static void stress_t4(void);
static void stress_t5(void);
static void stress_t6(void);
static void stress_t7(void);
static void stress_t8(void);
static void stress_t9(void);

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

    (void)mutex_init(&g_contended_mutex);

    if (task_create(stress_t0, 32u, 768u, "T0") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t1, 32u, 768u, "T1") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t2, 64u, 768u, "T2") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t3, 64u, 768u, "T3") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t4, 64u, 768u, "T4") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t5, 96u, 768u, "T5") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t6, 96u, 768u, "T6") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t7, 128u, 768u, "T7") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t8, 128u, 768u, "T8") == NULL) {
        while (1) {
        }
    }
    if (task_create(stress_t9, 160u, 768u, "T9") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void stress_body(uint32_t task_id)
{
    while (1) {
        (void)mutex_lock(&g_contended_mutex, RTOS_WAIT_FOREVER);
        g_mutex_acquisition_count[task_id]++;
        for (volatile uint32_t i = 0u; i < 1000u; i++) {
        }
        (void)mutex_unlock(&g_contended_mutex);
        (void)task_delay(1u + (task_id % 5u));
    }
}

static void stress_t0(void)
{
    stress_body(0u);
}

static void stress_t1(void)
{
    stress_body(1u);
}

static void stress_t2(void)
{
    stress_body(2u);
}

static void stress_t3(void)
{
    stress_body(3u);
}

static void stress_t4(void)
{
    stress_body(4u);
}

static void stress_t5(void)
{
    stress_body(5u);
}

static void stress_t6(void)
{
    stress_body(6u);
}

static void stress_t7(void)
{
    stress_body(7u);
}

static void stress_t8(void)
{
    stress_body(8u);
}

static void stress_t9(void)
{
    stress_body(9u);
}
