#ifndef MYALLOC_H
#define MYALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <intrin.h>

#define HEAP_SIZE       (1024 * 1024)
#define MAGIC_VALUE     0xDEADBEEF
#define ALIGNMENT       16
#define GUARD_SIZE      16
#define PAD_TO_DATA     16
#define BLOCK_OVERHEAD  (BLOCK_HEADER_SIZE + PAD_TO_DATA + GUARD_SIZE)

typedef struct Block {
    size_t size;
    int free;
    int magic;
    struct Block *next;
    struct Block *prev;
} Block_t;

#define BLOCK_HEADER_SIZE sizeof(Block_t)

void mi_init(void);
void *mi_malloc(size_t size);
void mi_free(void *ptr);

#ifdef MYALLOC_DEBUG
void mi_print_stats(void);
#else
#define mi_print_stats() ((void)0)
#endif

#endif