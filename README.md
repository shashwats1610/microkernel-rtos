# Cortex-M RTOS

![CI](https://github.com/shashwats1610/microkernel-rtos/actions/workflows/ci.yml/badge.svg)

Modular kernel sources, **monolithic firmware image** — a from-scratch preemptive RTOS for ARM Cortex-M4,
built to demonstrate real kernel internals instead of API wrappers. Interview-ready and intentionally
transparent: SVC bootstrap, PendSV context switching, priority scheduling with time slicing, full mutex
priority inheritance (including ready-queue migration), blocking timeouts, heap, and software timers.

## What this project proves

- Cortex-M exception-driven scheduling (`SVC` first dispatch + `PendSV` context switch).
- Scheduler invariants: 8 priority buckets, round-robin rings, configurable time slices.
- Synchronization: mutex with PI, semaphore, message queue — all with tick-based timeouts.
- Embedded safety boundaries: stack canaries, ISR constraints, static memory map.

## Architecture at a glance

- **Execution model**: Thread mode on PSP; handlers on MSP.
- **Preemption**: SysTick → tick, wake, timers, canary, time-slice check → PendSV.
- **Scheduling**: 8 buckets (`priority >> 5`), circular ready rings, `RTOS_TIME_SLICE_TICKS`.
- **Platform**: STM32F407-class linker map; QEMU `netduino2` for CI and smoke runs.

See [`docs/architecture.md`](docs/architecture.md) and [`docs/images/`](docs/images/).

## Quick verification

```bash
make clean && make all && make flash
make APP=mutex_demo && make debug    # GDB :1234 — inspect PI under mutex contention
make test-all                        # automated firmware tests (QEMU + GDB)
```

## Module map

| Path | Role |
|------|------|
| `include/` | Public API (`rtos.h`, `rtos_config.h`, `rtos_types.h`) |
| `src/core/` | Tasks, scheduler, context switch, blocking |
| `src/sync/` | Mutex (PI), semaphore |
| `src/ipc/` | Message queue |
| `src/memory/` | Static-pool first-fit heap |
| `src/timer/` | Software timers |
| `src/platform/` | SysTick, NVIC, syscalls |
| `platform/stm32f4/` | Startup + linker |
| `examples/` | `sched_demo`, `mutex_demo`, `stress_test` |
| `tests/` | Firmware integration tests |
| `scripts/run_tests.py` | CI/local test runner |

## Build

```bash
make                              # debug, QEMU (default)
make BUILD=release
make TARGET=hw BUILD=release      # hardware ELF (no QEMU_BUILD)
make APP=sched_demo|mutex_demo|stress_test|test_*
make flash / make debug           # QEMU
make flash-hw / make debug-hw     # ST-Link / OpenOCD (tools not bundled)
make test-all
make docs
```

## Features (implemented)

- Preemptive 8-bucket scheduler with round-robin and time slicing
- `task_delay`, `task_yield`, `task_delete` (with wait-list detach)
- Mutex priority inheritance with ready-bucket migration
- Semaphore, message queue, software timers
- Blocking API timeouts (`RTOS_ERR_TIMEOUT`)
- Stack canaries checked from SysTick
- Semihosting `printf` under QEMU; USART2 stub for `TARGET=hw`

## Out of scope

- Multi-architecture ports, MPU/process isolation, full STM32 HAL
- Safety certification (DO-178C / IEC 62304)
- Renode / cycle-accurate benchmarks (use silicon + DWT if needed)

## Known limitations

- QEMU `netduino2` approximates STM32F407; validate timing on hardware.
- Not a certified RTOS replacement.

## Documentation

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/api_reference.md`](docs/api_reference.md)
- [`docs/diagrams/README.md`](docs/diagrams/README.md)
- [`ENGINEERING_RETROSPECTIVE.txt`](ENGINEERING_RETROSPECTIVE.txt)

## License

MIT — see [`LICENSE`](LICENSE).
