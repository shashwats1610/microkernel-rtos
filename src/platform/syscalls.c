/**
 * @file syscalls.c
 * @brief newlib stubs: semihosting (QEMU) or USART2 polling TX (hardware).
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "rtos_config.h"

extern char _end;
extern uint32_t _estack;

void *_sbrk(ptrdiff_t incr)
{
    static char *heap_end;
    char *prev;

    if (heap_end == NULL) {
        heap_end = &_end;
    }

    prev = heap_end;
    if (heap_end + incr > (char *)(uintptr_t)&_estack - 4096) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev;
}

#if RTOS_QEMU_BUILD

static int semihost_sys_write(const char *buf, int len)
{
    struct {
        int fh;
        const char *buf;
        int len;
    } args;

    args.fh = 1;
    args.buf = buf;
    args.len = len;

    __asm volatile ("mov r0, #5\n\t"
                    "mov r1, %0\n\t"
                    "bkpt 0xAB"
                    :
                    : "r"(&args)
                    : "r0", "r1", "memory");
    return len;
}

int _write(int fd, char *ptr, int len)
{
    (void)fd;
    if ((ptr == NULL) || (len <= 0)) {
        return 0;
    }
    return semihost_sys_write(ptr, len);
}

#else

#define USART2_SR (*((volatile uint32_t *)0x40004400u))
#define USART2_DR (*((volatile uint32_t *)0x40004404u))

static void usart2_putc(char c)
{
    while ((USART2_SR & (1u << 7u)) == 0u) {
    }
    USART2_DR = (uint32_t)(uint8_t)c;
}

int _write(int fd, char *ptr, int len)
{
    int i;

    (void)fd;
    if ((ptr == NULL) || (len <= 0)) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        usart2_putc(ptr[i]);
    }
    return len;
}

#endif
