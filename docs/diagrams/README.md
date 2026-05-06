# Architecture diagrams (Draw.io)

Use these as interview visuals; commit sources here and optional PNGs under `docs/images/` if desired.

## Scheduler design (`scheduler.drawio`)

- Draw **8 priority buckets** (indices **0 = highest** … **7 = lowest**).
- Each bucket is a **circular singly linked list** of `TCB` nodes (`next_ready`).
- **`scheduler_get_next()`** scans buckets from **0** upward, takes the **head** task, **rotates** the ring pointer for round-robin.

## Context switch flow (`context_switch.drawio`)

- **SysTick** or **`task_yield`** sets **PENDSVSET** → **`PendSV_Handler`**.
- Save **`R4–R11`** on **PSP**, store PSP in **`current_tcb->stack_ptr`**.
- Call **`scheduler_get_next()`** (C).
- Restore **`R4–R11`**, update **PSP**, **`BX LR`** (exception return unstacks **R0–xPSR**).

## Memory layout (`memory_layout.drawio`)

- **Flash** `0x08000000` — vectors, `.text`, `.rodata`.
- **SRAM** `0x20000000` — `.data`, `.bss`, idle stack, heap pool, task stacks.
- Show stacks **growing down**, **canary** at **low address** of each stack region.

## Tips

- Keep diagrams **one page**; interviewers skim quickly.
- Annotate **tick rate**, **~145 cycles** estimate for PendSV path (see root `README.md`).
- Export **PNG** for slides; keep **`.drawio`** source in git for edits.
