# Implementation Plan

## Overview
Extender el asignador de memoria con **spin-lock para thread-safety**, **garantía de alineación a 16 bytes** y **guardrails 0xDEADBEEF** para detección de buffer overflow/underflow.

Actualmente el asignador usa `Block_t` de 32 bytes con 4 bytes de padding interno, alineación a 8 bytes solo por convención (no garantizada), y sin protección para multithreading ni detección de corrupción de memoria. Se requiere hardening para producción educativa con detección temprana de errores de programación.

## Scope
- Spin-lock con `_InterlockedExchange` (MSVC `intrin.h`) envuelve `mi_malloc` y `mi_free`
- Heap alineado a 16 bytes con `_Alignas(16)` (C11) o `__declspec(align(16))` (MSVC)
- Puntero de retorno de `mi_malloc` siempre 16-byte aligned
- Guardrail `0xDEADBEEF` dentro del header (ocupa el padding existente) y al final del bloque de datos
- Verificación de guardrails en `mi_free` con mensaje de error descriptivo
- Actualización de documentación y diagramas Mermaid en README.md

---

## Types
Una modificación interna a `Block_t` (cambia el padding por un campo `magic`) y nuevas constantes del sistema.

### Modificación de `Block_t`

**Actual (32 bytes):**
```c
typedef struct Block {
    size_t size;        // 0x00, 8 bytes
    int free;           // 0x08, 4 bytes
    int _padding;       // 0x0C, 4 bytes (padding)
    struct Block *next; // 0x10, 8 bytes
    struct Block *prev; // 0x18, 8 bytes
} Block_t; // total 32 bytes
```

**Nuevo (32 bytes — mismo tamaño):**
```c
typedef struct Block {
    size_t size;        // 0x00, 8 bytes — tamaño del bloque DE DATOS (sin overhead)
    int free;           // 0x08, 4 bytes — 1 = libre, 0 = ocupado
    int magic;          // 0x0C, 4 bytes — 0xDEADBEEF, guardrail del header
    struct Block *next; // 0x10, 8 bytes
    struct Block *prev; // 0x18, 8 bytes
} Block_t; // total 32 bytes
```

### Nuevas constantes

```c
#define MAGIC_VALUE     0xDEADBEEF       // Valor centinela en header y guardrails
#define ALIGNMENT       16               // Alineación garantizada (en bytes)
#define GUARD_SIZE      16               // Tamaño del guardrail post-datos
#define PAD_TO_DATA     16               // Padding después de header para alinear datos a 16
#define BLOCK_OVERHEAD  (BLOCK_HEADER_SIZE + PAD_TO_DATA + GUARD_SIZE)  // 64 bytes fijos por bloque
```

### Layout de un bloque ocupado

```
Offset  Contenido                     Tamaño
──────  ──────────────────────────    ─────
0x00    Block_t.size                  8
0x08    Block_t.free                  4
0x0C    Block_t.magic = 0xDEADBEEF    4  ← guardrail del header
0x10    Block_t.next                  8
0x18    Block_t.prev                  8
       ──────────────────────────         = 32 bytes (header)
0x20    Padding (0x00)                16  ← para alinear datos a 16
       ──────────────────────────         = 48 bytes hasta datos
0x30    Datos del usuario              size
0x30 + size  Guardrail post-datos     16  = 0xDEADBEEF * 4
       ──────────────────────────         = size + 64 bytes totales por bloque
```

### Verificación en `mi_free`

1. `block->magic != MAGIC_VALUE` → **ERROR**: "Buffer underflow o corrupción del header detectada (magic=%08x)"
2. Post-guardrail corrupto → **ERROR**: "Buffer overflow detectado en bloque %p (post-guardrail corrupto)"

---

## Files

### `Project v1/myalloc.h` — MODIFICAR
- Agregar `#include <intrin.h>` (para `_InterlockedExchange` y `_mm_pause`)
- Agregar constantes: `MAGIC_VALUE`, `ALIGNMENT`, `GUARD_SIZE`, `PAD_TO_DATA`, `BLOCK_OVERHEAD`
- `Block_t`: campo `_padding` → `int magic;`
- `mi_init`, `mi_malloc`, `mi_free`, `mi_print_stats` sin cambios de firma

### `Project v1/myalloc.c` — MODIFICAR completamente
- `static uint8_t heap[HEAP_SIZE]` → `static _Alignas(16) uint8_t heap[HEAP_SIZE]`
- Agregar: `static volatile long heap_lock = 0;`
- Agregar: funciones `static void lock_acquire(void)` y `static void lock_release(void)`
- `mi_init`: sin cambios lógicos
- `mi_malloc`:
  - Alinear `size` a 16 en vez de 8
  - `remaining` debe dejar espacio para `BLOCK_OVERHEAD` al fragmentar
  - El puntero de retorno ahora es: `(uint8_t*)curr + BLOCK_HEADER_SIZE + PAD_TO_DATA`
  - Escribir guardrail post-datos al asignar
  - Envolver con `lock_acquire()`/`lock_release()`
- `mi_free`:
  - Verificar `block->magic`
  - Verificar post-guardrail
  - Envolver con `lock_acquire()`/`lock_release()`
- `next_block`: la aritmética debe usar `block->size + BLOCK_OVERHEAD - GUARD_SIZE` (porque el post-guard pertenece al bloque actual, el next comienza después)
  - **Fórmula correcta**: `(uint8_t*)block + BLOCK_HEADER_SIZE + PAD_TO_DATA + block->size + GUARD_SIZE`
  - Simplificando: `(uint8_t*)block + BLOCK_HEADER_SIZE + PAD_TO_DATA + block->size + GUARD_SIZE`
  - O mejor: `(uint8_t*)block + BLOCK_OVERHEAD + block->size`
  - Donde `BLOCK_OVERHEAD = BLOCK_HEADER_SIZE + PAD_TO_DATA + GUARD_SIZE = 64`
- `mi_print_stats`: actualizar cálculo de Overhead

### `Project v1/main.c` — MODIFICAR
- Agregar prueba de buffer overflow detectado
- Agregar prueba de buffer underflow detectado
- (Opcional) Agregar prueba de concurrencia básica

### `Project v1/README.md` — MODIFICAR
- Agregar diagrama del nuevo layout de bloque con guardrails
- Agregar sección de "Detección de Buffer Overflow"
- Actualizar diagrama de estructura del Header

---

## Functions

### Nuevas funciones (internas en `myalloc.c`)

| Función | Firma | Propósito |
|---------|-------|-----------|
| `lock_acquire` | `static void lock_acquire(void)` | Adquiere el spin-lock usando `_InterlockedExchange` |
| `lock_release` | `static void lock_release(void)` | Libera el spin-lock |

### Funciones modificadas

| Función | Archivo | Cambios |
|---------|---------|---------|
| `next_block` | `myalloc.c` | Aritmética actualizada para nuevo layout: `offset = BLOCK_OVERHEAD + block->size` |
| `mi_init` | `myalloc.c` | Ajustar `size` inicial: `HEAP_SIZE - BLOCK_OVERHEAD` |
| `mi_malloc` | `myalloc.c` | Alinear a 16; usar `BLOCK_OVERHEAD` en cálculos de fragmentación; retornar `ptr = (uint8_t*)block + BLOCK_HEADER_SIZE + PAD_TO_DATA`; escribir post-guard; envolver con spin-lock |
| `mi_free` | `myalloc.c` | Verificar `block->magic`; verificar post-guardrail; envolver con spin-lock |
| `mi_print_stats` | `myalloc.c` | Overhead actualizado |

### Funciones sin cambios
`mi_init` — sin cambios de lógica interna (solo ajuste de tamaño inicial)

---

## Classes
`Block_t` en `myalloc.h` — un solo campo renombrado: `int _padding` → `int magic`. Sin cambios estructurales (mismo tamaño, mismos offsets).

---

## Dependencies
Ninguna nueva dependencia externa. Uso de intrínsecos de MSVC:
- `_InterlockedExchange` (de `intrin.h`)
- `_mm_pause` (de `intrin.h`, intrínseco SSE2 disponible en x64)

---

## Testing

### Pruebas nuevas en `main.c`

1. **Test: Buffer overflow detection**
   - Asignar bloque de 64 bytes
   - Escribir más allá del límite: `memset(ptr + 64, 0xFF, 32)`
   - Llamar `mi_free(ptr)` — debe imprimir error de overflow
   - Verificar que el programa continúa (no crash)

2. **Test: Header corruption detection (underflow from previous block)**
   - Asignar dos bloques adyacentes (A de 64, B de 64)
   - Escribir desde A más allá de su límite para corromper el magic de B
   - Llamar `mi_free(B)` — debe imprimir error de magic corrupto

3. **Test: Thread-safety smoke test** (opcional)
   - Crear 2 hilos que hagan alloc/free concurrente
   - Verificar que no hay doble free ni corrupción

### Pruebas existentes que deben seguir pasando
- Asignaciones básicas (100, 200, 50 B) con nueva alineación
- Liberación + Coalescing (verificar que los guardrails no interfieran)
- Fragmentación + First-Fit
- Asignación grande (512 KB) + fallo controlado
- Liberación total del heap (debe quedar 1 bloque free)

---

## Implementation Order

Secuencia de cambios para minimizar conflictos:

1. **Actualizar `myalloc.h`**: renombrar campo `_padding` → `magic`, agregar nuevas constantes, agregar `#include <intrin.h>`
2. **Actualizar `myalloc.c`**: 
   2a. Agregar `_Alignas(16)` al heap y variable del spin-lock
   2b. Implementar `lock_acquire` / `lock_release`
   2c. Actualizar `next_block` con nueva aritmética (`BLOCK_OVERHEAD`)
   2d. Actualizar `mi_init` (nuevo tamaño inicial)
   2e. Actualizar `mi_malloc` (alineación 16, guardrails, fragmentación, spin-lock)
   2f. Actualizar `mi_free` (verificación de guardrails, spin-lock)
   2g. Actualizar `mi_print_stats` (overhead)
3. **Actualizar `main.c`**: agregar pruebas de detección de overflow/underflow
4. **Compilar y ejecutar**: `cl.exe /std:c11 /Fe:allocator.exe main.c myalloc.c`
5. **Actualizar `README.md`**: nuevo diagrama de layout con guardrails y sección de detección de errores