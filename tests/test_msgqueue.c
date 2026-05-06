/**
 * @file test_msgqueue.c
 * @brief Message queue ping path between producer and consumer tasks.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rtos.h"

typedef struct {
    uint32_t seq;
} ping_msg_t;

static msg_queue_t g_q;
static uint8_t g_q_storage[sizeof(ping_msg_t) * 8u];

volatile uint32_t g_msgqueue_test_pass;

static void producer_task(void);
static void consumer_task(void);

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

    g_msgqueue_test_pass = 0u;

    if (msg_queue_init(&g_q, g_q_storage, sizeof(ping_msg_t), 8u) != RTOS_OK) {
        while (1) {
        }
    }

    if (task_create(consumer_task, 48u, 768u, "cons") == NULL) {
        while (1) {
        }
    }

    if (task_create(producer_task, 64u, 768u, "prod") == NULL) {
        while (1) {
        }
    }

    for (;;) {
        (void)rtos_start();
    }
}

static void producer_task(void)
{
    ping_msg_t m;

    while (1) {
        memset(&m, 0, sizeof(m));
        m.seq = 7u;
        if (msg_queue_send(&g_q, &m, 0u) == RTOS_OK) {
            g_msgqueue_test_pass = 1u;
        }
        (void)task_delay(20u);
    }
}

static void consumer_task(void)
{
    ping_msg_t out;

    while (1) {
        if (msg_queue_recv(&g_q, &out, 0u) == RTOS_OK) {
            if (out.seq == 7u) {
                g_msgqueue_test_pass = 2u;
            }
        }
    }
}
