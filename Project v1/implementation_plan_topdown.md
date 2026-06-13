# Implementation Plan — Reconstrucción Top-Down del Asignador de Memoria

## Overview

Reconstruir `myalloc.c` desde cero aplicando metodología Top-Down: primero el esqueleto compilable, luego la lógica de búsqueda, luego el motor de seguridad.

El código actual (v2) ya funciona con 20/20 tests, pero se desarrolló de forma iterativa con varios bugs intermedios. Esta reconstrucción aplica una metodología disciplinada donde cada fase produce un archivo que compila y puede verificarse antes de avanzar a la siguiente.

## Scope

- **Fase 1 — Esqueleto**: `myalloc.h` inmutable. `myalloc.c` solo con includes, defines, struct, variables globales, y funciones con bodies "stub" (que compilan pero no ejecutan lógica real). Compilar y verificar link.
- **Fase 2 — First-Fit + Alineación**: Implementar `mi_init`, `next_block`, `mi_malloc` con First-Fit y alineación a 16. Sin guardrails, sin coalescing. Compilar y probar alloc básico.
- **Fase 3 — Seguridad**: Agregar `lock_acquire`/`lock_release` (spin-lock), guardrails (header `magic` + post-guard), `write_guard`, `guard_ok`, detección en `mi_free`, coalescing. Agregar tests destructivos.

---

## Types

No hay cambios de tipos respecto a `myalloc.h`. Los tipos ya están definidos y no se modifican:

```c
typedef struct Block {
    size_t size;            // tamaño del bloque DE DATOS
    int free;               // 1 = libre, 0 = ocupado
    int magic;              // 0xDEADBEEF cuando ocupado, 0xBAADF00D cuando libre
    struct Block *next;     // siguiente bloque en lista
    struct Block *prev;     // bloque anterior en lista
} Block_t;                  // 32 bytes
```

Constantes definidas en `myalloc.h`:
- `HEAP_SIZE`, `MAGIC_VALUE`, `ALIGNMENT`, `GUARD_SIZE`, `PAD_TO_DATA`, `BLOCK_HEADER_SIZE`, `BLOCK_OVERHEAD`

Constantes internas de `myalloc.c`:
- `MAGIC_FREED 0xBAADF00D` — valor mágico de bloques liberados (detecta doble free)

---

## Files

### `Project v1/myalloc.h` — SIN CAMBIOS
El archivo actual ya está completo y correcto. No se toca.

### `Project v1/myalloc.c` — REESCRIBIR COMPLETAMENTE en 3 fases

| Fase | Fragmento | Descripción |
|------|-----------|-------------|
| 1 | Esqueleto | Includes, defines, variables globales, stubs de funciones que compilan |
| 2 | Búsqueda | `mi_init`, `next_block`, `mi_malloc` (sin guardrails, sin lock) |
| 3 | Seguridad | Spin-lock, guardrails, detección en `mi_free`, coalescing, `mi_print_stats` |

### `Project v1/main.c` — ACTUALIZAR en 3 fases
| Fase | Tests a incluir |
|------|-----------------|
| 1 | Mínimo: `mi_init()`, `mi_malloc(100)`, `mi_free(ptr)`, `mi_print_stats()` — solo verificar que compila y no crashea |
| 2 | Tests básicos de alloc/free, coalescing, fragmentación, alineación 16 B |
| 3 | Tests destructivos: overflow, underflow, doble free |

### `Project v1/README.md` — ACTUALIZAR AL FINAL
Agregar sección "Metodología Top-Down" con las 3 fases documentadas.

---

## Functions

### Fase 1 — Esqueleto (todas stub, compilan pero no operan)

| Función | Firma | Implementación |
|---------|-------|----------------|
| `lock_acquire` | `static void lock_acquire(void)` | `{ }` (cuerpo vacío) |
| `lock_release` | `static void lock_release(void)` | `{ }` (cuerpo vacío) |
| `next_block` | `static Block_t *next_block(Block_t *block)` | `{ return NULL; }` |
| `post_guard` | `static uint32_t *post_guard(Block_t *block)` | `{ return NULL; }` |
| `guard_ok` | `static int guard_ok(Block_t *block)` | `{ return 1; }` (siempre OK) |
| `write_guard` | `static void write_guard(Block_t *block)` | `{ }` (no-op) |
| `mi_init` | `void mi_init(void)` | Inicializa head como bloque libre único, sin guardrails |
| `mi_malloc` | `void *mi_malloc(size_t size)` | `{ return NULL; }` stub |
| `mi_free` | `void mi_free(void *ptr)` | `{ }` stub (no-op) |
| `mi_print_stats` | `void mi_print_stats(void)` | Imprime stats con valores fijos o vacío |

### Fase 2 — Lógica de Búsqueda

| Función | Cambio respecto a Fase 1 |
|---------|--------------------------|
| `next_block` | Implementar aritmética real: `(uint8_t*)block + BLOCK_HEADER_SIZE + PAD_TO_DATA + block->size + GUARD_SIZE` |
| `mi_init` | Inicializar `head->magic = MAGIC_VALUE` (aunque fase 2 no verifica guardrails, es correcto) |
| `mi_malloc` | Implementar First-Fit completo: recorrer lista, fragmentar si `remaining > BLOCK_OVERHEAD`, retornar `ptr = (uint8_t*)curr + BLOCK_HEADER_SIZE + PAD_TO_DATA`. Sin lock, sin guardrails. |
| `mi_free` | Implementar marcado como libre + coalescing (next y prev). Sin verificación de guardrails. |
| `mi_print_stats` | Implementar recorrido completo de lista con stats reales |

### Fase 3 — Motor de Seguridad

| Función | Cambio respecto a Fase 2 |
|---------|--------------------------|
| `lock_acquire` | Implementar spin-lock con `_InterlockedExchange` + `_mm_pause` |
| `lock_release` | Implementar con `_InterlockedExchange` |
| `post_guard` | Implementar: `(uint32_t*)((uint8_t*)block + BLOCK_HEADER_SIZE + PAD_TO_DATA + block->size)` |
| `guard_ok` | Implementar: verificar 4 uint32_t contra `MAGIC_VALUE` |
| `write_guard` | Implementar: escribir 4 uint32_t con `MAGIC_VALUE` |
| `mi_malloc` | Agregar `lock_acquire()`/`lock_release()` alrededor, escribir guardrails con `write_guard` |
| `mi_free` | Agregar `lock_acquire()`/`lock_release()`, verificar `magic` (underflow), `guard_ok` (overflow), `magic == MAGIC_FREED` (doble free) |

---

## Classes

`Block_t` — sin cambios. Ya está definido en `myalloc.h` y correcto.

---

## Dependencies

Ninguna nueva. Mismas que el código actual:
- `intrin.h` para `_InterlockedExchange` y `_mm_pause`
- `stdio.h` para `printf` en stats y errores
- `string.h` para `memset`
- `stdint.h` para tipos exactos

---

## Testing

### main.c — Progresión por fases

**Fase 1:**
```c
int main() {
    mi_init();
    mi_print_stats();        // debe mostrar 1 bloque FREE
    void *p = mi_malloc(64); // debe retornar NULL (stub)
    mi_free(p);              // no-op
    printf("Fase 1 OK\n");
    return 0;
}
```

**Fase 2:**
- Test 1: Alloc 100, 200, 50 bytes. Verificar punteros no NULL. Verificar alineación a 16.
- Test 2: Free + Coalescing. Liberar bloques adyacentes y verificar fusión.
- Test 3: Fragmentación + First-Fit. Alloc/free intercalados, verificar que asigna en primer hueco.

**Fase 3:**
- Test 4 (de Fase 2): Large alloc 512KB + fallo controlado.
- Test 5 (de Fase 2): Free all.
- Test 6: Doble free (debe ser silencioso).
- Test 7: Buffer overflow detection (corromper post-guardrail → error).
- Test 8: Header corruption / underflow (corromper magic → error).

---

## Implementation Order

Pasos en orden estricto. Cada paso produce un archivo que compila sin errores (solo warnings menores de MSVC con `%zu`).

1. **Escribir `myalloc.c` Fase 1** — esqueleto completo con stubs. Compilar: `cl.exe /std:c11 /c myalloc.c`
2. **Escribir `main.c` Fase 1** — test mínimo. Compilar y link: `cl.exe /std:c11 /Fe:allocator.exe main.c myalloc.c`. Ejecutar `.\allocator.exe`.
3. **Expandir `myalloc.c` a Fase 2** — implementar `next_block`, `mi_init`, `mi_malloc`, `mi_free`, `mi_print_stats`. Compilar.
4. **Expandir `main.c` a Fase 2** — tests de alloc/free/coalescing/fragmentación/alineación. Compilar, link, ejecutar.
5. **Expandir `myalloc.c` a Fase 3** — agregar spin-lock, guardrails, detección de errores en `mi_free`. Compilar.
6. **Expandir `main.c` a Fase 3** — tests destructivos (overflow, underflow, doble free). Compilar, link, ejecutar.
7. **Actualizar `README.md`** — agregar sección de metodología Top-Down con las 3 fases documentadas.