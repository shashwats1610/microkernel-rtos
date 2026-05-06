# STM32F4 platform glue

Linker script `linker.ld` targets 1 MiB flash / 128 KiB SRAM (main SRAM only). Startup `startup_stm32f407xx.s` provides reset + vector table.
