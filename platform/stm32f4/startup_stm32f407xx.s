/**
 * @file startup_stm32f407xx.s
 * @brief Reset handler, vector table, data/bss init for Cortex-M4 (STM32F407-class).
 */

.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_estack
    msr msp, r0

    movs r0, #0
    msr control, r0

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
    subs r2, r2, r1
    ble .L_datadone
.L_copyloop:
    ldr r3, [r0], #4
    str r3, [r1], #4
    subs r2, r2, #4
    bgt .L_copyloop
.L_datadone:

    ldr r0, =_sbss
    ldr r1, =_ebss
    subs r1, r1, r0
    movs r2, #0
    ble .L_bssdone
.L_bssloop:
    str r2, [r0], #4
    subs r1, r1, #4
    bgt .L_bssloop
.L_bssdone:

    bl SystemInit
    bl main
.L_forever:
    b .L_forever
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b Default_Handler
.size Default_Handler, .-Default_Handler

.section .vectors,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVCall_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    /* IRQ 0..79 STM32F407 - weak aliases */
    .rept 82
    .word Default_Handler
    .endr

.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler

.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler

.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler

.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler

.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler

.weak SVCall_Handler
.thumb_set SVCall_Handler, Default_Handler

.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler

.weak PendSV_Handler
.thumb_set PendSV_Handler, Default_Handler

.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler
