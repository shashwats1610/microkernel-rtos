/**
 * @file heap.h
 * @brief First-fit heap over a static pool.
 */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize heap structures (call once before allocations).
 */
void heap_init(void);

/**
 * @brief Allocate memory from the RTOS heap.
 *
 * @param size Requested size (aligned up to 8 bytes).
 * @return Pointer or NULL on failure.
 */
void *heap_alloc(size_t size);

/**
 * @brief Free memory previously allocated from the heap.
 *
 * @param ptr Pointer from @ref heap_alloc.
 */
void heap_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_H */
