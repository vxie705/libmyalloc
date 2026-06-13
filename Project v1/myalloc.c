#define MYALLOC_DEBUG
#include "myalloc.h"
#define MAGIC_FREED 0xBAADF00D
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static _Alignas(16) uint8_t heap[HEAP_SIZE];
static Block_t *head = NULL;
static volatile long heap_lock = 0;

static void lock_acquire(void) {
    while (_InterlockedExchange(&heap_lock, 1) != 0) {
        _mm_pause();
    }
}

static void lock_release(void) {
    _InterlockedExchange(&heap_lock, 0);
}

static Block_t *next_block(Block_t *block) {
    uint8_t *next_addr = (uint8_t *)block + BLOCK_OVERHEAD + block->size;
    uint8_t *heap_end = heap + HEAP_SIZE;
    if (next_addr >= heap_end) return NULL;
    return (Block_t *)next_addr;
}

static uint32_t *post_guard(Block_t *block) {
    return (uint32_t *)((uint8_t *)block + BLOCK_HEADER_SIZE + PAD_TO_DATA + block->size);
}

static int guard_ok(Block_t *block) {
    uint32_t *pg = post_guard(block);
    return (pg[0] == MAGIC_VALUE && pg[1] == MAGIC_VALUE &&
            pg[2] == MAGIC_VALUE && pg[3] == MAGIC_VALUE);
}

static void write_guard(Block_t *block) {
    uint32_t *pg = post_guard(block);
    pg[0] = MAGIC_VALUE;
    pg[1] = MAGIC_VALUE;
    pg[2] = MAGIC_VALUE;
    pg[3] = MAGIC_VALUE;
}

void mi_init(void) {
    head = (Block_t *)heap;
    head->size = HEAP_SIZE - BLOCK_OVERHEAD;
    head->free = 1;
    head->magic = MAGIC_VALUE;
    head->next = NULL;
    head->prev = NULL;
}

void *mi_malloc(size_t size) {
    if (size == 0) return NULL;

    if (size % ALIGNMENT != 0) {
        size += (ALIGNMENT - (size % ALIGNMENT));
    }

    lock_acquire();

    Block_t *curr = head;
    while (curr != NULL) {
        if (curr->free && curr->size >= size) {
            size_t remaining = curr->size - size;
            if (remaining > BLOCK_OVERHEAD) {
                Block_t *new_block = (Block_t *)((uint8_t *)curr + BLOCK_OVERHEAD + size);
                new_block->size = remaining - BLOCK_OVERHEAD;
                new_block->free = 1;
                new_block->magic = MAGIC_VALUE;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (curr->next != NULL) {
                    curr->next->prev = new_block;
                }
                curr->next = new_block;
            }

            curr->size = size;
            curr->free = 0;
            curr->magic = MAGIC_VALUE;
            write_guard(curr);
            void *ptr = (void *)((uint8_t *)curr + BLOCK_HEADER_SIZE + PAD_TO_DATA);

            lock_release();
            return ptr;
        }
        curr = curr->next;
    }

    lock_release();
    return NULL;
}

void mi_free(void *ptr) {
    if (ptr == NULL) return;

    lock_acquire();

    Block_t *block = (Block_t *)((uint8_t *)ptr - BLOCK_HEADER_SIZE - PAD_TO_DATA);

    if (block->magic == MAGIC_FREED) {
        lock_release();
        return;
    }

    if (block->magic != MAGIC_VALUE) {
        printf("ERROR: Buffer underflow o corrupcion del header detectada "
               "(block=%p, magic=0x%08X, se esperaba 0x%08X)\n",
               (void *)block, block->magic, MAGIC_VALUE);
        lock_release();
        return;
    }

    if (!guard_ok(block)) {
        printf("ERROR: Buffer overflow detectado en bloque %p\n", (void *)block);
        lock_release();
        return;
    }

    if (block->free) {
        lock_release();
        return;
    }

    block->free = 1;
    block->magic = MAGIC_FREED;

    Block_t *next = next_block(block);
    if (next != NULL && next->free) {
        block->size += BLOCK_OVERHEAD + next->size;
        block->next = next->next;
        if (next->next != NULL) {
            next->next->prev = block;
        }
    }

    Block_t *prev = block->prev;
    if (prev != NULL && prev->free) {
        prev->size += BLOCK_OVERHEAD + block->size;
        prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = prev;
        }
    }

    lock_release();
}

void mi_print_stats(void) {
    Block_t *curr = head;
    int index = 0;
    size_t total_free = 0;
    size_t total_used = 0;
    int free_count = 0;
    int used_count = 0;

    printf("\n=== HEAP STATE ===\n");
    while (curr != NULL) {
        const char *status = curr->free ? "FREE" : "USED";
        size_t data_size = curr->size;
        printf("  Block %2d | data=%6zu bytes | %s\n", index, data_size, status);
        if (curr->free) {
            total_free += data_size;
            free_count++;
        } else {
            total_used += data_size;
            used_count++;
        }
        curr = curr->next;
        index++;
    }
    printf("------------------\n");
    printf("  Total blocks : %d\n", index);
    printf("  Free blocks  : %d (total %zu bytes data)\n", free_count, total_free);
    printf("  Used blocks  : %d (total %zu bytes data)\n", used_count, total_used);
    printf("  Overhead     : %zu bytes (header+pad+guard per block)\n",
           (size_t)index * BLOCK_OVERHEAD);
    printf("  Total heap   : %zu bytes\n\n", HEAP_SIZE);
}