# Architecture diagrams

| Source | Description |
|--------|-------------|
| [`scheduler.mmd`](scheduler.mmd) | 8 buckets, circular ready rings |
| [`context_switch.mmd`](context_switch.mmd) | SysTick → PendSV flow |
| [`../images/scheduler.svg`](../images/scheduler.svg) | Scheduler overview |
| [`../images/context_switch.svg`](../images/context_switch.svg) | Context switch flow |

Export PNG locally: `npx @mermaid-js/mermaid-cli -i scheduler.mmd -o ../images/scheduler.png`

PendSV path is on the order of ~100–200 CPU cycles on Cortex-M4 (measure with DWT CYCCNT on silicon).
