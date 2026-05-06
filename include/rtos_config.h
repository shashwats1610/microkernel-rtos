/**
 * @file rtos_config.h
 * @brief Compile-time RTOS limits and tuning.
 */

#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#include <stdint.h>

/** @brief SysTick frequency in Hz (1 ms tick). */
#define RTOS_TICK_RATE_HZ      (1000u)

/** @brief Number of distinct priority levels used by the scheduler (0 = highest). */
#define RTOS_NUM_PRIORITIES    (8u)

/** @brief Maximum tasks (excluding idle). */
#define RTOS_MAX_TASKS         (32u)

/** @brief Minimum stack size in bytes for @ref task_create. */
#define RTOS_MIN_STACK_SIZE    (256u)

/** @brief Idle task stack size in bytes (storage array is 8-byte aligned). */
#define RTOS_IDLE_STACK_SIZE   (4096u)

/** @brief Infinite timeout for blocking APIs that take timeout in ticks. */
#define RTOS_WAIT_FOREVER      (0u)

/** @brief Maximum length of task name including terminating null. */
#define RTOS_TASK_NAME_MAX     (16u)

/** @brief Logical priority range stored in TCB (must map into 8 levels). */
#define RTOS_PRIORITY_MIN      (0u)
#define RTOS_PRIORITY_MAX      (255u)

/** @brief Heap pool size for dynamic stacks (bytes), 8-byte aligned. */
#define RTOS_HEAP_POOL_SIZE    (32768u)

/** @brief Fixed message size for message queues (bytes). */
#define RTOS_MSG_SIZE_DEFAULT  (32u)

/** @brief Message queue depth default. */
#define RTOS_MSG_QUEUE_DEPTH   (16u)

/** @brief Maximum software timers. */
#define RTOS_MAX_SW_TIMERS     (16u)

/** @brief Stack overflow canary value (placed at low stack addresses). */
#define RTOS_STACK_CANARY      (0xDEADBEEFu)

/** @brief Enable mutex priority inheritance (recommended). */
#define RTOS_MUTEX_PRIORITY_INHERITANCE  (1u)

/** @brief Time slice for round-robin within same priority (ticks). */
#define RTOS_TIME_SLICE_TICKS  (1u)

/** @brief Build with relaxed clock for QEMU smoke testing (-DQEMU_BUILD). */
#if defined(QEMU_BUILD)
#define RTOS_QEMU_BUILD        (1)
#else
#define RTOS_QEMU_BUILD        (0)
#endif

#endif /* RTOS_CONFIG_H */
