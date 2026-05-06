/**
 * @file system.c
 * @brief Minimal CMSIS-like clock and SysTick for STM32F407 / QEMU bring-up.
 */

#include <stdint.h>

#include "rtos_config.h"

uint32_t SystemCoreClock = 16000000u;

/**
 * @brief CMSIS SystemInit hook called from startup before main().
 */
void SystemInit(void)
{
}

extern void rtos_tick_increment(void);
extern void rtos_wake_delayed(void);
extern void rtos_stack_check_all(void);
extern void scheduler_mark_reschedule(void);
extern void sw_timer_tick_process(void);

#define SYSTICK_CTRL (*((volatile uint32_t *)0xE000E010u))
#define SYSTICK_LOAD (*((volatile uint32_t *)0xE000E014u))
#define SYSTICK_VAL (*((volatile uint32_t *)0xE000E018u))

/**
 * @brief Configure SysTick as a 24-bit decrementing timer (CMSIS-compatible subset).
 *
 * @param ticks Number of ticks between interrupts (reload value is ticks-1).
 * @return 0 on success, 1 if ticks out of range.
 */
uint32_t SysTick_Config(uint32_t ticks)
{
    if ((ticks - 1u) > 0xFFFFFFu) {
        return 1u;
    }

    SYSTICK_LOAD = ticks - 1u;
    SYSTICK_VAL = 0u;
    SYSTICK_CTRL = (1u << 2u) | (1u << 1u) | (1u << 0u);
    return 0u;
}

/**
 * @brief Set priority for core exceptions or NVIC interrupts (stub if unsure).
 *
 * @param irqn IRQ number (unused in stub).
 * @param preempt_prio Priority byte (unused in stub).
 */
void nvic_set_priority(int32_t irqn, uint32_t preempt_prio)
{
    (void)irqn;
    (void)preempt_prio;
}

/**
 * @brief Initialize clocks: full PLL path on hardware; HSI default for QEMU_BUILD.
 */
void SystemInitClock(void)
{
#if RTOS_QEMU_BUILD
    SystemCoreClock = 16000000u;
#else
    /* Placeholder: assume bootloader/HAL already configured PLL to 168 MHz. */
    SystemCoreClock = 168000000u;
#endif
}

/**
 * @brief SysTick ISR: tick, wake sleeping tasks, timers, stack check, reschedule.
 */
void SysTick_Handler(void)
{
    rtos_tick_increment();
    rtos_wake_delayed();
    sw_timer_tick_process();
    rtos_stack_check_all();
    scheduler_mark_reschedule();
}
