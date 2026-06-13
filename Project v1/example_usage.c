#include <stdio.h>
#include <string.h>
#include "myalloc.h"

int main(void) {
    printf("=== myalloc - Ejemplo de Integracion ===\n");
    printf("Nivel: B (Libreria Estatica .lib)\n\n");

    mi_init();

    printf("Asignando 3 bloques...\n");
    char *a = (char *)mi_malloc(64);
    char *b = (char *)mi_malloc(128);
    char *c = (char *)mi_malloc(256);

    if (!a || !b || !c) {
        printf("ERROR: Fallo en asignacion\n");
        return 1;
    }

    strcpy(a, "Hola desde myalloc!");
    strcpy(b, "Segundo bloque - 128 bytes");
    strcpy(c, "Tercer bloque - 256 bytes");

    printf("  a: \"%s\" (64 bytes)\n", a);
    printf("  b: \"%s\" (128 bytes)\n", b);
    printf("  c: \"%s\" (256 bytes)\n", c);

    mi_print_stats();

    printf("\nLiberando bloques b y c...\n");
    mi_free(b);
    mi_free(c);

    mi_print_stats();

    printf("\nAsignando bloque de 200 bytes...\n");
    char *d = (char *)mi_malloc(200);
    if (d) {
        strcpy(d, "Este bloque aprovecha el coalescing!");
        printf("  d: \"%s\"\n", d);
    }

    mi_print_stats();

    printf("\nLiberando todo...\n");
    mi_free(a);
    mi_free(d);

    mi_print_stats();

    printf("\n=== Ejemplo completado exitosamente ===\n");
    return 0;
}