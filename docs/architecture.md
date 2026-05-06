# Architecture

## Execution environment

- **Privileged handler mode** uses **MSP** for exception stacks (including PendSV/SVC prologue stacks).
- **Thread mode tasks** use **PSP**. The first thread entry uses **SVC** with an exception return that unstacks the synthetic frame prepared in `task_create()`.
- **SysTick** drives a 1 ms tick (`RTOS_TICK_RATE_HZ`), increments global time, wakes delayed tasks, advances software timers, checks stack canaries, then requests **PendSV** for preemption.

## Memory map (STM32F407-class link)

| Region | Address | Size | Notes |
|--------|---------|------|-------|
| Flash | `0x08000000` | 1 MiB | `.text`, `.rodata` |
| SRAM | `0x20000000` | 128 KiB | `.data`, `.bss`, stacks, RTOS heap pool |

CCM SRAM (`0x10000000`) is **not modeled** here to keep the Phase-1 linker narrative simple.

## Scheduler

Eight ready rings indexed by `prio_bucket = priority >> 5` (clamped). Each ring is **circular** via `TCB::next_ready`.

`scheduler_get_next()` selects the lowest-index nonempty bucket, takes the head task as the next runnable, and **rotates** the ring pointer so round-robin progresses fairly within the bucket.

## Context switch

`PendSV_Handler` (see `src/core/context_switch.s`) saves **R4–R11** to the current PSP stack, stores PSP into `current_tcb->stack_ptr`, invokes `scheduler_get_next()`, then restores **R4–R11** for the successor task and updates PSP to point at the hardware exception frame.

## Stack integrity

Each task stack reserves a **canary word** `0xDEADBEEF` at the lowest allocated address (`stack_base`). `SysTick_Handler` walks registered tasks and traps if corrupted.

## Known simplifications

- NVIC priority programming is stubbed in `nvic_set_priority()` for portability.
- Mutex priority inheritance adjusts numeric priority fields but does **not** physically reorder ready-queue placement when inheritance boosts an owner (acceptable for the demonstration scope; production kernels would migrate the owner TCB between buckets).
