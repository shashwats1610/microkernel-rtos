/**
 * @file system.c
 * @brief CMSIS-like clock, NVIC, and SysTick for STM32F407 / QEMU.
 */

#include <stdint.h>

#include "rtos_config.h"
#include "scheduler.h"

uint32_t SystemCoreClock = 16000000u;

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

#define SCB_SHPR_BASE ((volatile uint8_t *)0xE000ED18u)
#define NVIC_IPR_BASE ((volatile uint8_t *)0xE000E400u)

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

void nvic_set_priority(int32_t irqn, uint32_t preempt_prio)
{
    volatile uint8_t *reg;

    if (irqn < 0) {
        /* CMSIS: SCB->SHP[((uint32_t)IRQn & 0xF) - 4] */
        reg = SCB_SHPR_BASE + ((((uint32_t)irqn) & 0xFUL) - 4u);
    }
    else {
        reg = NVIC_IPR_BASE + (uint32_t)irqn;
    }

    *reg = (uint8_t)(preempt_prio & 0xFFu);
}

void SystemInitClock(void)
{
#if RTOS_QEMU_BUILD
    SystemCoreClock = 16000000u;
#else
    /*
     * Hardware: enable HSE + PLL for 168 MHz (STM32F407).
     * Assumes HSE=8 MHz; adjust if your board differs.
     */
    volatile uint32_t *RCC_CR = (volatile uint32_t *)0x40023800u;
    volatile uint32_t *RCC_PLLCFGR = (volatile uint32_t *)0x40023804u;
    volatile uint32_t *RCC_CFGR = (volatile uint32_t *)0x40023808u;
    volatile uint32_t *FLASH_ACR = (volatile uint32_t *)0x40023C00u;

    *RCC_CR |= (1u << 16u);
    while ((*RCC_CR & (1u << 17u)) == 0u) {
    }

    *FLASH_ACR = (5u << 0u) | (1u << 8u) | (1u << 9u) | (1u << 10u);
    *RCC_PLLCFGR = (8u << 0u) | (0u << 6u) | (192u << 6u) | (0u << 16u) |
                   (4u << 24u) | (0u << 22u);
    *RCC_CR |= (1u << 24u);
    while ((*RCC_CR & (1u << 25u)) == 0u) {
    }

    *RCC_CFGR |= (2u << 0u);
    while ((*RCC_CFGR & (3u << 2u)) != (2u << 2u)) {
    }

    SystemCoreClock = 168000000u;
#endif
}

void SysTick_Handler(void)
{
    rtos_tick_increment();
    rtos_wake_delayed();
    sw_timer_tick_process();
    rtos_stack_check_all();
    scheduler_time_slice_tick();
    scheduler_mark_reschedule();
}
