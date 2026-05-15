# Architecture

## Execution environment

- **Privileged handler mode** uses **MSP** for exception stacks.
- **Thread mode tasks** use **PSP**. First entry uses **SVC** with synthetic frame from `task_create()`.
- **SysTick** drives 1 ms ticks: wake delayed/blocked tasks, software timers, stack canaries, time-slice check, then **PendSV**.

## Memory map (STM32F407-class link)

| Region | Address | Size | Notes |
|--------|---------|------|-------|
| Flash | `0x08000000` | 1 MiB | `.text`, `.rodata` |
| SRAM | `0x20000000` | 128 KiB | `.data`, `.bss`, stacks, heap |

CCM SRAM (`0x10000000`) is not modeled in the linker script.

## Scheduler

Eight ready rings: `prio_bucket = priority >> 5`. Each ring is circular via `TCB::next_ready`.

`scheduler_get_next()` scans buckets 0..7, takes head, rotates ring for round-robin.

**Time slicing**: `scheduler_time_slice_tick()` runs from SysTick; when `time_slice_counter >= RTOS_TIME_SLICE_TICKS` and another task exists in the same bucket, PendSV is pended.

**Priority inheritance**: `mutex.c` boosts owner `priority` and calls `scheduler_rebucket_task()` when the bucket changes while the owner is READY.

## Context switch

`PendSV_Handler` saves **R4–R11** on PSP, stores PSP in `current_tcb->stack_ptr`, calls `scheduler_get_next()`, restores successor registers and PSP.

## Blocking and timeouts

`rtos_block_current()` sets `wake_time` and `block_reason`. `rtos_wake_delayed()` dispatches:

- `BLOCK_DELAY` → ready queue
- `BLOCK_MUTEX` / `BLOCK_SEMAPHORE` / `BLOCK_MSG_*` → module timeout wake (wait list unlink, `RTOS_ERR_TIMEOUT`)

## Stack integrity

Canary `0xDEADBEEF` at `stack_base`; `rtos_stack_check_all()` from SysTick traps with debug globals on corruption.

## Platform

- **QEMU** (`TARGET=qemu`): 16 MHz, semihosting `_write`.
- **Hardware** (`TARGET=hw`): PLL bring-up in `SystemInitClock()`, USART2 polling TX stub.
