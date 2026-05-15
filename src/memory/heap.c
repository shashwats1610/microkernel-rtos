/**
 * @file heap.c
 * @brief First-fit allocator with explicit free list (ISR must not call alloc/free).
 */

#include "heap.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rtos_config.h"

typedef struct heap_block_s {
    uint32_t size;
    struct heap_block_s *next;
} heap_block_t;

static uint8_t heap_pool[RTOS_HEAP_POOL_SIZE] __attribute__((aligned(8)));
static heap_block_t *free_list;
static bool heap_ready;

static void _remove_from_list(heap_block_t *blk, heap_block_t *prev)
{
    if (prev == NULL) {
        free_list = blk->next;
    }
    else {
        prev->next = blk->next;
    }
    blk->next = NULL;
}

static void _prepend_free(heap_block_t *blk)
{
    blk->next = free_list;
    free_list = blk;
}

static void _coalesce_forward(void)
{
    heap_block_t *walk;

    walk = free_list;
    while ((walk != NULL) && (walk->next != NULL)) {
        uint8_t *end;

        end = (uint8_t *)walk + walk->size;
        if (end == (uint8_t *)walk->next) {
            walk->size += walk->next->size;
            walk->next = walk->next->next;
            continue;
        }
        walk = walk->next;
    }
}

/**
 * @brief Initialize heap structures (call once before allocations).
 */
void heap_init(void)
{
    memset(heap_pool, 0, sizeof(heap_pool));
    heap_ready = false;
    free_list = (heap_block_t *)(void *)heap_pool;
    free_list->size = sizeof(heap_pool);
    free_list->next = NULL;
    heap_ready = true;
}

/**
 * @brief Allocate memory from the RTOS heap.
 *
 * @param size Requested size (aligned up to 8 bytes).
 * @return Pointer or NULL on failure.
 */
void *heap_alloc(size_t size)
{
    heap_block_t *prev;
    heap_block_t *curr;
    size_t total;

    if (!heap_ready || size == 0U) {
        return NULL;
    }

    total = (size + 7U) & ~(size_t)7U;
    total += sizeof(heap_block_t);

    prev = NULL;
    curr = free_list;
    while (curr != NULL) {
        if (curr->size >= total) {
            size_t leftover;

            leftover = curr->size - total;
            _remove_from_list(curr, prev);
            if (leftover >= (sizeof(heap_block_t) + 8U)) {
                heap_block_t *frag;

                frag = (heap_block_t *)((uint8_t *)curr + total);
                frag->size = leftover;
                frag->next = NULL;
                curr->size = total;
                _prepend_free(frag);
                _coalesce_forward();
            }
            return (uint8_t *)curr + sizeof(heap_block_t);
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * @brief Free memory previously allocated from the heap.
 *
 * @param ptr Pointer from @ref heap_alloc.
 */
void heap_free(void *ptr)
{
    heap_block_t *blk;

    if ((ptr == NULL) || !heap_ready) {
        return;
    }

    blk = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    _prepend_free(blk);
    _coalesce_forward();
}
