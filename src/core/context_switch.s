/**
 * @file context_switch.s
 * @brief PendSV exception for preemptive context switch (PSP, R4-R11).
 */

.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.extern scheduler_get_next
.extern current_tcb

.section .text.SVC_Handler
.global SVC_Handler
.global SVCall_Handler
.type SVC_Handler, %function
.thumb_func
SVC_Handler:
    bl      scheduler_get_next
    ldr     r2, [r0]
    ldmia   r2!, {r4-r11}
    msr     psp, r2
    movs    r1, #0
    msr     basepri, r1
    cpsie   i
    ldr     lr, =0xFFFFFFFD
    bx      lr

.thumb_set SVCall_Handler, SVC_Handler

.size SVC_Handler, .-SVC_Handler

.section .text.PendSV_Handler
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func
PendSV_Handler:
    cpsid   i
    mrs     r0, psp
    cbz     r0, .L_no_save
    stmdb   r0!, {r4-r11}
    ldr     r2, =current_tcb
    ldr     r2, [r2]
    cbz     r2, .L_skip_store
    str     r0, [r2]
.L_skip_store:
.L_no_save:
    push    {lr}
    bl      scheduler_get_next
    pop     {lr}
    ldr     r2, [r0]
    ldmia   r2!, {r4-r11}
    msr     psp, r2
    cpsie   i
    bx      lr

.size PendSV_Handler, .-PendSV_Handler
