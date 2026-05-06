/**
 * @file task.c
 * @brief Task creation, idle task, delay, and kernel start.
 */

#include "task.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "heap.h"
#include "rtos_config.h"
#include "scheduler.h"
#include "sw_timer.h"

#define NVIC_PRIO_BITS       (4u)

static TCB_t *task_registry[RTOS_MAX_TASKS + 1u];
static uint32_t task_registry_count;

static uint32_t idle_stack_words[RTOS_IDLE_STACK_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(8)));
static void _idle_task_fn(void);
static void _task_exit_trap(void);

static uint8_t _prio_bucket(uint8_t priority)
{
    uint8_t bucket;

    bucket = (uint8_t)(priority >> 5);
    if (bucket >= RTOS_NUM_PRIORITIES) {
        bucket = RTOS_NUM_PRIORITIES - 1u;
    }
    return bucket;
}

static void _registry_add(TCB_t *tcb)
{
    if ((task_registry_count < (RTOS_MAX_TASKS + 1u)) && (tcb != NULL)) {
        task_registry[task_registry_count] = tcb;
        task_registry_count++;
    }
}

static uint32_t *_build_initial_stack(uint32_t *stack_high_exclusive,
                                    void (*task_func)(void))
{
    uint32_t *sp;

    sp = stack_high_exclusive;

    /* Hardware stacked frame (low address .. high): R0,R1,R2,R3,R12,LR,PC,xPSR */
    sp -= 8;
    sp[0] = 0u;
    sp[1] = 0u;
    sp[2] = 0u;
    sp[3] = 0u;
    sp[4] = 0u;
    sp[5] = (uint32_t)_task_exit_trap;
    sp[6] = (uint32_t)task_func;
    sp[7] = 0x01000000u;

    /* Manual registers R4-R11 below hardware frame */
    sp -= 8;
    sp[0] = 0u;
    sp[1] = 0u;
    sp[2] = 0u;
    sp[3] = 0u;
    sp[4] = 0u;
    sp[5] = 0u;
    sp[6] = 0u;
    sp[7] = 0u;

    return sp;
}

static rtos_status_t _task_build_common(TCB_t *tcb, void (*task_func)(void))
{
    uint32_t *high;
    uint8_t *base_u8;

    if ((tcb == NULL) || (task_func == NULL)) {
        return RTOS_ERR_PARAM;
    }

    base_u8 = (uint8_t *)tcb->stack_base;
    high = (uint32_t *)(void *)(base_u8 + tcb->stack_size);
    high = (uint32_t *)((uintptr_t)high & ~(uintptr_t)7u);

    *(uint32_t *)(void *)base_u8 = RTOS_STACK_CANARY;

    tcb->stack_ptr = _build_initial_stack(high, task_func);
    tcb->task_func = task_func;
    tcb->next_ready = NULL;
    tcb->base_priority = tcb->priority;
    tcb->blocked_on_mutex = NULL;
    tcb->mutex_wait_next = NULL;
    tcb->time_slice_counter = 0u;
    return RTOS_OK;
}

/**
 * @brief Initialize kernel task subsystem (calls scheduler + idle creation).
 *
 * @return Status.
 */
rtos_status_t task_init(void)
{
    uint32_t idle_stack_bytes;

    heap_init();
    sw_timer_init();
    scheduler_init();
    task_registry_count = 0u;

    idle_tcb = (TCB_t *)heap_alloc(sizeof(TCB_t));
    if (idle_tcb == NULL) {
        return RTOS_ERR_NO_MEM;
    }

    memset(idle_tcb, 0, sizeof(TCB_t));
    idle_stack_bytes = (uint32_t)sizeof(idle_stack_words);
    idle_tcb->stack_base = (uint32_t *)(void *)&idle_stack_words[0];
    idle_tcb->stack_size = idle_stack_bytes;
    idle_tcb->priority = 255u;
    idle_tcb->prio_bucket = RTOS_NUM_PRIORITIES - 1u;
    (void)memcpy(idle_tcb->name, "idle", 5);

    if (_task_build_common(idle_tcb, _idle_task_fn) != RTOS_OK) {
        return RTOS_ERR_STATE;
    }

    idle_tcb->state = TASK_STATE_READY;
    if (scheduler_add_task(idle_tcb) != RTOS_OK) {
        return RTOS_ERR_STATE;
    }

    _registry_add(idle_tcb);
    return RTOS_OK;
}

/**
 * @brief Create a new task.
 *
 * @param task_func Pointer to task entry function.
 * @param priority Task priority (0 = highest logical per config mapping).
 * @param stack_size Stack size in bytes (minimum @ref RTOS_MIN_STACK_SIZE).
 * @param name Task name (max @ref RTOS_TASK_NAME_MAX including null).
 * @return Pointer to TCB, or NULL on failure.
 */
TCB_t *task_create(void (*task_func)(void), uint8_t priority, uint32_t stack_size,
                   const char *name)
{
    TCB_t *tcb;
    uint32_t *stk;
    uint8_t *stk_bytes;

    if ((task_func == NULL) || (stack_size < RTOS_MIN_STACK_SIZE)) {
        return NULL;
    }

    if (task_registry_count >= RTOS_MAX_TASKS) {
        return NULL;
    }

    stack_size = (stack_size + 7u) & ~7u;

    tcb = (TCB_t *)heap_alloc(sizeof(TCB_t));
    stk_bytes = (uint8_t *)heap_alloc(stack_size);
    if ((tcb == NULL) || (stk_bytes == NULL)) {
        heap_free(tcb);
        heap_free(stk_bytes);
        return NULL;
    }

    stk = (uint32_t *)(void *)stk_bytes;

    memset(tcb, 0, sizeof(TCB_t));
    tcb->stack_base = stk;
    tcb->stack_size = stack_size;
    tcb->priority = priority;
    tcb->prio_bucket = _prio_bucket(priority);

    if (name != NULL) {
        (void)memcpy(tcb->name, name, RTOS_TASK_NAME_MAX - 1u);
        tcb->name[RTOS_TASK_NAME_MAX - 1u] = '\0';
    }
    else {
        tcb->name[0] = '\0';
    }

    if (_task_build_common(tcb, task_func) != RTOS_OK) {
        heap_free(stk_bytes);
        heap_free(tcb);
        return NULL;
    }

    tcb->state = TASK_STATE_READY;
    if (scheduler_add_task(tcb) != RTOS_OK) {
        heap_free(stk_bytes);
        heap_free(tcb);
        return NULL;
    }

    _registry_add(tcb);
    return tcb;
}

volatile const char *g_rtos_stack_overflow_task_name;
volatile uint32_t g_rtos_stack_overflow_canary;

/**
 * @brief Delete a task (caller must not delete currently running task from itself).
 *
 * @param tcb Task control block.
 * @return Status.
 */
rtos_status_t task_delete(TCB_t *tcb)
{
    if (tcb == NULL) {
        return RTOS_ERR_PARAM;
    }

    /*
     * Proper implementation would scan every mutex, semaphore, and message queue
     * to detach @a tcb from wait lists. For this portfolio scope, callers must
     * only delete tasks that are READY or SUSPENDED (not blocked on a primitive).
     */
    if (tcb->state == TASK_STATE_BLOCKED) {
        return RTOS_ERR_STATE;
    }

    if (tcb == idle_tcb) {
        return RTOS_ERR_STATE;
    }

    if (tcb == current_tcb) {
        return RTOS_ERR_STATE;
    }

    scheduler_remove_task(tcb);
    tcb->state = TASK_STATE_TERMINATED;
    heap_free(tcb->stack_base);
    heap_free(tcb);
    return RTOS_OK;
}

/**
 * @brief Yield CPU to the scheduler.
 */
void task_yield(void)
{
    scheduler_mark_reschedule();
}

static volatile uint32_t s_rtos_tick_count;

/**
 * @brief Number of elapsed system ticks (1 ms default).
 *
 * @return Tick count (wraps uint32_t).
 */
uint32_t rtos_get_tick(void)
{
    return s_rtos_tick_count;
}

/**
 * @brief Delay current task for a number of ticks.
 *
 * @param ticks Number of system ticks (must be > 0).
 * @return Status.
 */
rtos_status_t task_delay(uint32_t ticks)
{
    TCB_t *self;
    uint32_t wake;

    if (ticks == 0u) {
        return RTOS_ERR_PARAM;
    }

    self = (TCB_t *)current_tcb;
    if (self == NULL) {
        return RTOS_ERR_STATE;
    }

    __asm volatile ("cpsid i" ::: "memory");
    wake = s_rtos_tick_count + ticks;
    self->wake_time = wake;
    self->state = TASK_STATE_BLOCKED;
    scheduler_remove_task(self);
    __asm volatile ("cpsie i" ::: "memory");

    scheduler_mark_reschedule();

    while (self->state == TASK_STATE_BLOCKED) {
        __NOP();
    }

    return RTOS_OK;
}

void rtos_tick_increment(void);

/**
 * @brief Wake delayed tasks whose wake time has elapsed.
 */
void rtos_wake_delayed(void)
{
    uint32_t i;
    uint32_t now;

    now = s_rtos_tick_count;
    for (i = 0; i < task_registry_count; i++) {
        TCB_t *t;

        t = task_registry[i];
        if ((t == NULL) || (t == idle_tcb)) {
            continue;
        }

        if ((t->state == TASK_STATE_BLOCKED) && (now >= t->wake_time)) {
            t->state = TASK_STATE_READY;
            if (scheduler_add_task(t) != RTOS_OK) {
                /* duplicate safety */
            }
        }
    }
}

/**
 * @brief Check stack canaries for all registered tasks (called from SysTick).
 */
void rtos_stack_check_all(void)
{
    uint32_t i;

    for (i = 0; i < task_registry_count; i++) {
        TCB_t *t;
        uint32_t *base;

        t = task_registry[i];
        if ((t == NULL) || (t->stack_base == NULL)) {
            continue;
        }

        base = t->stack_base;
        if (*base != RTOS_STACK_CANARY) {
            /*
             * Stop here under GDB: inspect g_rtos_stack_overflow_task_name and
             * g_rtos_stack_overflow_canary. Production code would log or reset.
             */
            g_rtos_stack_overflow_task_name = t->name;
            g_rtos_stack_overflow_canary = *base;
            while (1) {
            }
        }
    }
}

/**
 * @brief Kernel SysTick tick hook (increment counter).
 */
void rtos_tick_increment(void)
{
    s_rtos_tick_count++;
}

static void _idle_task_fn(void)
{
    while (1) {
        __asm volatile ("wfi");
    }
}

static void _task_exit_trap(void)
{
    while (1) {
        __asm volatile ("nop");
    }
}

extern uint32_t SystemCoreClock;
extern void SystemInitClock(void);
extern void nvic_set_priority(int32_t irqn, uint32_t preempt_prio);
extern uint32_t SysTick_Config(uint32_t ticks);

/**
 * @brief Enter multitasking: configure SysTick and perform first dispatch.
 *
 * @return Does not return if successful.
 */
rtos_status_t rtos_start(void)
{
    if (idle_tcb == NULL) {
        return RTOS_ERR_STATE;
    }

    SystemInitClock();

    nvic_set_priority(-14, (1u << (NVIC_PRIO_BITS - 1u)));
    nvic_set_priority(-7, (uint32_t)(1u << NVIC_PRIO_BITS));

    if (SysTick_Config(SystemCoreClock / RTOS_TICK_RATE_HZ) != 0u) {
        return RTOS_ERR_STATE;
    }

    __asm volatile ("cpsie i" ::: "memory");
    __asm volatile ("svc 0" ::: "memory");

    while (1) {
        __asm volatile ("nop");
    }
}
