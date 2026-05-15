# STM32F407 platform files

- `startup_stm32f407xx.s` — vector table and reset handler
- `linker.ld` — 1 MiB flash @ `0x08000000`, 128 KiB SRAM @ `0x20000000`

## QEMU (default)

```bash
make TARGET=qemu flash
```

## Hardware

```bash
make TARGET=hw BUILD=release all
make flash-hw    # requires st-flash
```

USART2 stub in `syscalls.c` (polling TX). Enable USART2 + GPIOA clocks and pinmux in your board init if you use `printf` on silicon.

PLL sequence in `SystemInitClock()` when `TARGET=hw` (assumes 8 MHz HSE).
