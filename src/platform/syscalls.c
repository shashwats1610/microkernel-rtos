/**
 * @file syscalls.c
 * @brief Minimal newlib stubs (heap allocator is internal; malloc unused by kernel).
 */

#include <errno.h>
#include <stddef.h>
#include <unistd.h>

/**
 * @brief Stub character output for hosted libraries (replace with UART if needed).
 */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    (void)ptr;
    (void)len;
    return len;
}
