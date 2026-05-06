# Microkernel RTOS

Interview-oriented preemptive RTOS for **ARM Cortex-M4** (STM32F407-class), with a minimal bring-up path under **QEMU** (`netduino2`) for CI-friendly smoke testing.

**Repository:** [github.com/shashwats1610/microkernel-rtos](https://github.com/shashwats1610/microkernel-rtos)

## About this project

This is a **from-scratch**, **portfolio-grade** real-time kernel aimed at **embedded systems interviews** (startups and teams that care about scheduling, context switch cost, and basic IPC). It is **not** a certified safety RTOS; it is a **concise, readable** codebase you can walk through on a whiteboard: PendSV-based preemption, eight priority levels with round-robin within a level, mutex priority inheritance (demonstrated in `examples/mutex_demo`), and a small set of primitives (semaphores, message queues, software timers, heap).

**Why build it:** To show that you understand **Cortex-M exception model** (MSP vs PSP, SVC first dispatch, PendSV for deferred context switch), **scheduler invariants**, and **common embedded pitfalls** (stack alignment, ISR-safe rules, volatile/shared data, stack canaries).

## Target platform

| Item | Detail |
|------|--------|
| **MCU class** | STM32F407-equivalent (Cortex-M4F, 168 MHz target on hardware) |
| **Link map** | 1 MiB Flash @ `0x08000000`, 128 KiB SRAM @ `0x20000000` (main SRAM only; CCM not modeled) |
| **Simulation** | QEMU `-M netduino2` (STM32F2-class; **approximate** — use [Renode](https://renode.io/) for closer STM32 models) |
| **Toolchain** | `arm-none-eabi-gcc`, newlib nano, GNU Make |

## Features

- **Tasks**: TCB with stack canaries, eight scheduler priority buckets (maps logical `0–255` priorities), idle task.
- **Scheduler**: Preemptive priority scheduling with **round-robin** inside each bucket (implemented by rotating each bucket’s ready ring when selecting).
- **Context switch**: **PendSV** saves/restores **R4–R11** against **PSP**; **SVC** performs the first dispatch into the initial thread context.
- **IPC / sync**: Mutex with **basic priority inheritance**, counting/binary semaphores, fixed-size message queues.
- **Heap**: First-fit allocator over a static pool (**no** libc heap use inside ISRs).
- **Timers**: Sorted (by expiry) software timers invoked from the SysTick path.
- **Time**: `task_delay()`, global tick counter, wake sorted blocked tasks in SysTick.

## Repository layout

- `include/` — Public headers (`rtos.h`, `rtos_config.h`, `rtos_types.h`).
- `src/core/` — Scheduler, tasks, PendSV assembly.
- `src/platform/` — Clock/SysTick hooks, startup glue.
- `platform/stm32f4/` — Linker script and startup vector table.
- `examples/` — `blinky`, `mutex_demo` (PI narrative), `stress_test` (10 tasks + mutex contention).
- `tests/` — Standalone firmware tests (`APP=` selector in the Makefile).
- `docs/` — Architecture notes, API overview, diagram checklist for interviews.

Start with **`docs/architecture.md`** for memory map and scheduler design, then **`src/core/context_switch.s`** for the exception path.

## Performance metrics

### Context switch latency

**Estimated: ~145 CPU cycles** (instruction-level analysis; validate with **DWT CYCCNT** on hardware).

Approximate breakdown (ARM Cortex-M4, Thumb-2):

| Stage | Cycles (approx.) |
|-------|------------------|
| Exception entry (hardware stacking) | ~12 |
| Manual `R4–R11` save (`STMDB`) | ~8 |
| Save PSP into current TCB | ~6 |
| `scheduler_get_next()` (scan buckets, RR) | ~40–60 |
| Manual restore (`LDMIA`) | ~8 |
| Exception return | ~12 |

**Total ~145 cycles** (~0.86 µs at 168 MHz). Default **QEMU** builds use **16 MHz** (`-DQEMU_BUILD`); re-time on silicon.

### Memory footprint (order of magnitude)

- **Kernel + libc nano**: run `arm-none-eabi-size build/firmware.elf` after `make all`.
- **Per-task overhead**: TCB (`sizeof(TCB_t)`) + chosen stack size from heap.
- **Heap pool**: `RTOS_HEAP_POOL_SIZE` (see [`include/rtos_config.h`](include/rtos_config.h)).

### Known simplifications (portfolio scope)

- **Mutex PI**: Boosts owner **priority/prio_bucket** but does **not** physically migrate the owner between ready-queue buckets (production kernels would reinsert the owner).
- **Timeouts**: Passed to blocking APIs; **full wake-by-timeout** not wired everywhere (`mutex_lock`, `semaphore_wait`, message queues).
- **Stack overflow**: Detected via canary; stops in a dedicated loop with **`g_rtos_stack_overflow_task_name`** / **`g_rtos_stack_overflow_canary`** set for GDB (see below). Production would UART-log or reset.
- **`task_delete`**: Returns **`RTOS_ERR_STATE`** if the task is **`TASK_STATE_BLOCKED`** (wait-list detachment not implemented).

### Debugging stack overflow

When corruption is detected, execution stops in **`rtos_stack_check_all()`** with:

- **`g_rtos_stack_overflow_task_name`** — name field of the offending TCB (see [`src/core/task.h`](src/core/task.h)).
- **`g_rtos_stack_overflow_canary`** — value read at stack base (should differ from `RTOS_STACK_CANARY`).

Set a breakpoint on that loop or watch those globals in GDB.

## Why not vs FreeRTOS

This repository is a **learning and portfolio** kernel. It is **not** positioned as a drop-in substitute for FreeRTOS, Zephyr, or ThreadX: those kernels represent decades of silicon-specific tuning, trace tooling, and MISRA-qualified variants. Benchmarking against them would be **misleading** without matching configs, trace sinks, and workloads.

## Build prerequisites

- GNU Arm Embedded Toolchain: `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`
- `make` (GNU Make)
- Optional: QEMU (`qemu-system-arm`) for `make flash` / `make test`

## Build modes

The Makefile exposes:

- **Debug** (default): `-O0 -g3`
- **Release**: `make BUILD=release` → `-O2 -g`

Avoid `-O3` for embedded timing assumptions unless you re-validate every ISR boundary.

## Build & test

### Build

```bash
make clean
make all              # expect zero warnings with -Wall -Wextra
make APP=mutex_demo
make APP=stress_test
```

### Verify (after successful link)

```bash
arm-none-eabi-size build/firmware.elf
arm-none-eabi-nm build/firmware.elf | findstr " U "   # Windows; use grep on Unix
```

Undefined symbols should be limited to what your newlib/BSP expects.

### Run / debug

```bash
make flash            # QEMU smoke (see Makefile for exact command)
make debug            # QEMU + GDB stub ; then: arm-none-eabi-gdb build/firmware.elf -ex "target remote :1234"
```

**mutex_demo / PI**: Break on `g_demo_phase`, `mutex_lock`, or inspect `current_tcb->priority` while Task L holds the mutex and Task H blocks.

**stress_test**: After **10 000+ ticks**, inspect **`g_mutex_acquisition_count[]`** — entries should advance over time.

## Build & run (QEMU)

Default application is **blinky**:

```bash
make clean && make
make flash   # qemu-system-arm -M netduino2 -kernel build/firmware.elf -nographic -serial stdio
```

Switch firmware images:

```bash
make APP=mutex_demo
make APP=stress_test
make APP=test_scheduler
```

**Important**: `-M netduino2` models an STM32F2-class MCU (Cortex-M3). It is an **approximate** integration vehicle. On-chip memories differ from STM32F407; this project links for **1 MiB flash @ 0x08000000** and **128 KiB SRAM @ 0x20000000** (CCM at `0x10000000` intentionally unused—see `docs/architecture.md`). For closer STM32 modeling consider **Renode**.

## Tests

Standalone tests are **firmware applications** that expose `volatile` status globals (inspect in GDB/QEMU). Run:

```bash
make test   # builds APP=test_scheduler then launches QEMU
```

UART **PASS/FAIL** strings require wiring `_write()` in `src/platform/syscalls.c` to your UART driver.

## Docs

- `docs/architecture.md` — Design notes and assumptions.
- `docs/api_reference.md` — Module-oriented API summary (see also Doxygen HTML).
- `docs/diagrams/README.md` — Interview diagram checklist (Draw.io).
- `make docs` — Requires `doxygen` on PATH.

## Performance methodology

Cycle-accurate PendSV measurements belong on **hardware** using **DWT CYCCNT** (guarded by `DEMCR.TRCENA`). QEMU does not faithfully model cycle counts for STM32-class silicon.

## License

See `LICENSE` (MIT).
