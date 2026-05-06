# API reference (overview)

Full declarations live in headers under `include/` and module headers under `src/**`. Generate HTML with `make docs` (Doxygen).

## Kernel control (`src/core/task.h`)

| Symbol | Purpose |
|--------|---------|
| `task_init()` | Heap + scheduler + idle task creation |
| `task_create()` | Instantiate a task with stack allocation |
| `task_delete()` | Tear down a non-running task |
| `task_delay()` | Block until tick deadline |
| `task_yield()` | Cooperative reschedule via PendSV |
| `rtos_start()` | Configure SysTick and launch first thread (`svc`) |
| `rtos_get_tick()` | Monotonic tick counter |

## Scheduler (`src/core/scheduler.h`)

Internal APIs consumed by assembly (`scheduler_get_next`, `scheduler_mark_reschedule`). Applications should use `task_*` wrappers.

## Mutex (`src/sync/mutex.h`)

| Symbol | Purpose |
|--------|---------|
| `mutex_init()` | Construct mutex |
| `mutex_lock()` | Acquire (blocking; PI optional via `RTOS_MUTEX_PRIORITY_INHERITANCE`) |
| `mutex_unlock()` | Release / wake highest waiter |

## Semaphores (`src/sync/semaphore.h`)

| Symbol | Purpose |
|--------|---------|
| `semaphore_init()` | Counting semaphore |
| `semaphore_init_binary()` | Binary semaphore |
| `semaphore_wait()` | Down operation |
| `semaphore_post()` | Up operation |

## Message queues (`src/ipc/msgqueue.h`)

| Symbol | Purpose |
|--------|---------|
| `msg_queue_init()` | Bind storage buffer |
| `msg_queue_send()` | Blocking enqueue |
| `msg_queue_recv()` | Blocking dequeue |

## Heap (`src/memory/heap.h`)

| Symbol | Purpose |
|--------|---------|
| `heap_init()` | Prepare pool |
| `heap_alloc()` / `heap_free()` | First-fit allocator |

## Software timers (`src/timer/sw_timer.h`)

| Symbol | Purpose |
|--------|---------|
| `sw_timer_start_once()` | One-shot |
| `sw_timer_start_periodic()` | Periodic |
| `sw_timer_stop()` | Cancel |
