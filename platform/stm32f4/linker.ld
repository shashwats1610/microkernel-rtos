/**
 * @file linker.ld
 * @brief STM32F407 memory layout: 1MB Flash @ 0x08000000, 128KB SRAM @ 0x20000000.
 *
 * CCM at 0x10000000 is intentionally unused (see docs/architecture.md).
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx): ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        . = ALIGN(8);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(8);
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > RAM

    /* Heap limit for newlib (_sbrk); RTOS pool is separate in heap.c */
    _end = .;
    PROVIDE(end = .);

    /DISCARD/ :
    {
        *(.comment)
    }
}
