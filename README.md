# Microkernel RTOS

From-scratch preemptive RTOS for ARM Cortex-M, built to demonstrate real kernel internals
instead of API wrappers. The codebase is interview-ready and intentionally transparent:
SVC bootstrap, PendSV context switching, priority scheduling, priority inheritance, heap, and
software timers.

## What this project proves

- You can implement Cortex-M exception-driven scheduling (`SVC` first dispatch + `PendSV`
  context switch path).
- You can design and explain scheduler invariants (priority buckets + round-robin fairness).
- You can build and validate practical synchronization primitives (mutex PI, semaphore,
  message queue).
- You can reason about embedded safety boundaries (stack canary checks, ISR constraints,
  static memory map assumptions).

## Architecture at a glance

- **Execution model**: Thread mode tasks run on PSP; handlers run on MSP.
- **Preemption path**: SysTick updates time/wakeups and pends PendSV, PendSV saves/restores
  callee registers and switches tasks, SVC performs first task dispatch.
- **Scheduling policy**: 8 ready buckets (`priority >> 5`) with ring rotation for
  round-robin inside each bucket.
- **Platform model**: Linked for STM32F407-class memory layout, with QEMU `netduino2`
  smoke-run support.

See [`docs/architecture.md`](docs/architecture.md) for design rationale and
[`src/core/context_switch.s`](src/core/context_switch.s) for the assembly switch path.

## Quick verification (5-10 minutes)

### 1) Build and smoke run

```bash
make clean
make all
make flash
```

### 2) Prove scheduler + PI behavior

```bash
make APP=mutex_demo
make debug
```

In GDB, inspect `current_tcb->priority` while high-priority task waits on a mutex held by a
low-priority task. This demonstrates the priority inheritance path in `src/sync/mutex.c`.

### 3) Prove test target flow

```bash
make APP=test_scheduler
make flash
```

Tests are standalone firmware apps selected by `APP` in the Makefile.
`make test` builds `APP=test_scheduler` and launches QEMU.

## Module map

- `include/` - public kernel API and configuration (`rtos.h`, `rtos_types.h`,
  `rtos_config.h`).
- `src/core/` - task lifecycle, scheduler implementation, context switch assembly.
- `src/sync/` - mutex (with PI) and semaphore implementations.
- `src/ipc/` - fixed-size message queue implementation.
- `src/memory/` - static pool first-fit allocator with coalescing.
- `src/timer/` - software timer list and tick integration.
- `src/platform/` - SysTick/system glue and syscall stubs.
- `platform/stm32f4/` - startup and linker script for memory layout.
- `examples/` - behavior demos (`blinky`, `mutex_demo`, `stress_test`).
- `tests/` - firmware test entrypoints (`test_scheduler`, `test_mutex`,
  `test_priority_inheritance`, `test_msgqueue`).

## Build and run workflow

- Build profile:
  - `make` (debug, `-O0 -g3`)
  - `make BUILD=release` (`-O2 -g`)
- App/test selection:
  - `make APP=blinky`
  - `make APP=mutex_demo`
  - `make APP=stress_test`
  - `make APP=test_scheduler`
- Run/debug:
  - `make flash` for QEMU smoke run
  - `make debug` for QEMU with GDB stub on `:1234`

## Engineering decisions and tradeoffs

- **Scheduler structure**: Ready rings keep per-bucket operations simple and deterministic.
- **PI scope**: Mutex PI boosts owner priority fields; this portfolio version does not migrate
  owner node between ready buckets.
- **Timeout support**: Timeout parameters exist in blocking APIs, but full wake-by-timeout
  coverage is intentionally limited in this phase.
- **Stack safety**: Canary checks run from SysTick and trap with debug globals for root-cause.

## Known limitations

- QEMU `netduino2` is an approximation for STM32F407 behavior; use Renode/hardware for tighter
  platform fidelity.
- Cycle-accurate timing claims must be measured on silicon (for example with DWT CYCCNT), not
  QEMU wall-clock timing.
- This is a learning/portfolio kernel and not a safety-certified RTOS replacement.

## Deep-dive docs

- [`docs/architecture.md`](docs/architecture.md) - execution model, memory map, and scheduler.
- [`docs/api_reference.md`](docs/api_reference.md) - API-oriented module documentation.
- [`docs/diagrams/README.md`](docs/diagrams/README.md) - diagram checklist for walkthroughs.
- [`ENGINEERING_RETROSPECTIVE.txt`](ENGINEERING_RETROSPECTIVE.txt) - engineering lessons,
  tradeoffs, and improvements.

Generate API docs with:

```bash
make docs
```

## License

MIT. See [`LICENSE`](LICENSE).
