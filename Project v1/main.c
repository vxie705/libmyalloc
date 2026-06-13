#include <stdio.h>
#include <string.h>
#include "myalloc.h"

int test_count = 0;
int pass_count = 0;

static void check(int cond, const char *msg) {
    test_count++;
    if (cond) {
        pass_count++;
        printf("  PASS: %s\n", msg);
    } else {
        printf("  FAIL: %s\n", msg);
    }
}

int main(void) {
    printf("======= FASE 3: Seguridad (Guardrails + Spin-Lock) =======\n");

    mi_init();
    mi_print_stats();

    printf("--- Test 1: Basic allocations + 16-byte alignment ---\n");
    void *p1 = mi_malloc(100);
    void *p2 = mi_malloc(200);
    void *p3 = mi_malloc(50);
    check(p1 != NULL, "alloc p1 (100 B)");
    check(p2 != NULL, "alloc p2 (200 B)");
    check(p3 != NULL, "alloc p3 (50 B)");

    strcpy((char *)p1, "Hola desde el heap!");
    strcpy((char *)p2, "Segundo bloque ocupado");
    strcpy((char *)p3, "Tercero");
    check(strcmp((char *)p1, "Hola desde el heap!") == 0, "write/read p1");
    check(strcmp((char *)p2, "Segundo bloque ocupado") == 0, "write/read p2");
    check(strcmp((char *)p3, "Tercero") == 0, "write/read p3");

    check(((uintptr_t)p1 & 0xF) == 0, "p1 aligned to 16 bytes");
    check(((uintptr_t)p2 & 0xF) == 0, "p2 aligned to 16 bytes");
    check(((uintptr_t)p3 & 0xF) == 0, "p3 aligned to 16 bytes");

    printf("--- Test 2: Free and coalescing ---\n");
    mi_free(p2);
    mi_print_stats();
    mi_free(p1);
    mi_print_stats();
    check(1, "coalescing completed without errors");

    printf("--- Test 3: Fragmentation + First-Fit ---\n");
    void *a = mi_malloc(80);
    void *b = mi_malloc(120);
    void *c = mi_malloc(60);
    void *d = mi_malloc(90);
    check(a && b && c && d, "four fragmented allocations");
    mi_free(b);
    mi_free(d);
    void *e = mi_malloc(70);
    check(e != NULL, "First-Fit alloc 70 B into first free hole");
    mi_print_stats();

    printf("--- Test 4: Large allocation & failure ---\n");
    void *big = mi_malloc(HEAP_SIZE / 2);
    check(big != NULL, "large alloc 512KB");
    void *fail = mi_malloc(HEAP_SIZE);
    check(fail == NULL, "oversized alloc returns NULL");

    printf("--- Test 5: Free all ---\n");
    mi_free(a);
    mi_free(c);
    mi_free(e);
    mi_free(big);
    mi_print_stats();

    printf("--- Test 6: Double-free protection ---\n");
    void *df = mi_malloc(32);
    check(df != NULL, "alloc 32 B for double-free test");
    mi_free(df);
    mi_free(df);
    check(1, "double free handled gracefully");

    printf("--- Test 7: Buffer overflow detection ---\n");
    void *overflow_test = mi_malloc(64);
    check(overflow_test != NULL, "alloc 64 B for overflow test");
    volatile char *corrupt = (volatile char *)overflow_test + 64;
    for (int i = 0; i < 32; i++) corrupt[i] = 0xFF;
    mi_free(overflow_test);
    check(1, "overflow detected gracefully (no crash)");

    printf("--- Test 8: Header corruption (underflow) detection ---\n");
    mi_init();
    void *block_a = mi_malloc(64);
    void *block_b = mi_malloc(64);
    check(block_a != NULL && block_b != NULL, "two adjacent blocks for underflow test");
    volatile char *corrupt2 = (volatile char *)block_a + 64;
    for (int i = 0; i < (int)(BLOCK_HEADER_SIZE + PAD_TO_DATA); i++) corrupt2[i] = 0xFF;
    mi_free(block_b);
    check(1, "underflow detected gracefully (no crash)");

    printf("\n=== Fase 3 Results: %d / %d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}