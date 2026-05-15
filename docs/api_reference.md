# API reference (overview)

Headers: `include/` and module headers under `src/**`. HTML: `make docs`.

## Kernel control (`src/core/task.h`)

| Symbol | Purpose |
|--------|---------|
| `task_init()` | Heap + scheduler + idle |
| `task_create()` | New task with heap stack |
| `task_delete()` | Teardown (detaches wait lists if blocked) |
| `task_delay()` | Block for ticks |
| `task_yield()` | Cooperative PendSV |
| `rtos_start()` | SysTick + first `svc` |
| `rtos_get_tick()` | Monotonic tick count |

## Scheduler (internal)

`scheduler_get_next`, `scheduler_rebucket_task`, `scheduler_time_slice_tick` — used by kernel and assembly.

## Mutex / semaphore / message queue

Blocking APIs take `timeout_ticks` (`0` = `RTOS_WAIT_FOREVER`). Return `RTOS_ERR_TIMEOUT` on expiry.

## Heap / software timers

`heap_alloc` / `heap_free`; `sw_timer_start_once` / `sw_timer_start_periodic`.

## Testing

`make test-all` runs `scripts/run_tests.py` (QEMU + GDB pass-flag checks).
