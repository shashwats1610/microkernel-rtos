/**
 * @file rtos_types.h
 * @brief RTOS object types and enumerations.
 */

#ifndef RTOS_TYPES_H
#define RTOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#include "rtos_config.h"

struct tcb_s;

/**
 * @brief Why a task is blocked (used for timeout wake and delete cleanup).
 */
typedef enum {
    BLOCK_NONE = 0,
    BLOCK_DELAY,
    BLOCK_MUTEX,
    BLOCK_SEMAPHORE,
    BLOCK_MSG_SEND,
    BLOCK_MSG_RECV
} block_reason_t;

/**
 * @brief Task scheduling state.
 */
typedef enum {
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED
} task_state_t;

/**
 * @brief RTOS API status codes.
 */
typedef enum {
    RTOS_OK = 0,
    RTOS_ERR_PARAM,
    RTOS_ERR_NO_MEM,
    RTOS_ERR_STATE,
    RTOS_ERR_TIMEOUT,
    RTOS_ERR_FULL,
    RTOS_ERR_EMPTY,
    RTOS_ERR_OWNER,
    RTOS_ERR_BUSY
} rtos_status_t;

/**
 * @brief Task Control Block.
 */
typedef struct tcb_s {
    uint32_t *stack_ptr;
    uint8_t prio_bucket;
    uint8_t priority;
    task_state_t state;
    uint32_t *stack_base;
    uint32_t stack_size;
    char name[RTOS_TASK_NAME_MAX];
    uint32_t wake_time;
    struct tcb_s *next;

    uint8_t base_priority;
    void *blocked_on_mutex;
    struct tcb_s *mutex_wait_next;

    void (*task_func)(void);
    struct tcb_s *next_ready;
    uint32_t time_slice_counter;

    block_reason_t block_reason;
    void *block_object;
    rtos_status_t block_wake_status;
} TCB_t;

#endif /* RTOS_TYPES_H */
