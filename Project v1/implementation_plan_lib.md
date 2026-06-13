# Implementation Plan — Conversión a Librería Estática (.lib)

## Overview

Convertir el asignador de memoria (`myalloc.c` / `myalloc.h`) en una **librería estática profesional (.lib)** con build script, separación de debug/release, y programa de ejemplo demostrando los 3 niveles de integración (código fuente, .lib, .dll comentado).

Actualmente el proyecto usa integración por código fuente: se compila `myalloc.c` junto con `main.c` cada vez. Una librería estática permite compilar `myalloc.c` una sola vez, generar un `.lib`, y que cualquier programa futuro solo necesite `myalloc.h` + `myalloc.lib` para usarlo.

## Scope

1. **Refactor `myalloc.h`**: Agregar `#ifdef MYALLOC_DEBUG` para condicionalmente incluir `mi_print_stats` (debug only). Separar API pública (estable) de debug.
2. **Crear `build.bat`**: Script que compila `myalloc.c` → `myalloc.obj` → `myalloc.lib` usando `lib.exe` de MSVC.
3. **Crear `example_usage.c`**: Programa ejemplo que demuestra los 3 niveles de integración (solo nivel B activo, nivel C comentado).
4. **Actualizar `main.c`**: Limpiar para que use `myalloc.lib` en vez de compilar `myalloc.c` directamente.
5. **Crear `Makefile`** (opcional): Para entornos GCC/MinGW.
6. **Actualizar `README.md`**: Agregar sección de "Integración como Librería Estática".

---

## Types

No hay cambios en `Block_t` ni en las constantes públicas. Solo se agrega una macro de control de compilación:

```c
#ifdef MYALLOC_DEBUG
    void mi_print_stats(void);
#else
    #define mi_print_stats() ((void)0)  // no-op en release
#endif
```

---

## Files

### `Project v1/myalloc.h` — MODIFICAR
- Envolver `mi_print_stats` en `#ifdef MYALLOC_DEBUG`

### `Project v1/myalloc.c` — SIN CAMBIOS
El código actual ya es correcto. No se modifica.

### `Project v1/main.c` — SIN CAMBIOS
Sigue siendo el test suite. Se compilará contra la librería.

### `Project v1/build.bat` — NUEVO
Script de build para MSVC que:
1. Inicializa entorno VS
2. Compila `myalloc.c` → `myalloc.obj` (versión release, sin debug)
3. Compila `myalloc_dbg.obj` (versión debug, con `-DMYALLOC_DEBUG`)
4. Crea `myalloc.lib` desde `myalloc.obj`
5. Crea `myalloc_dbg.lib` desde `myalloc_dbg.obj`
6. Compila `main.c` linkeando con `myalloc.lib` → `allocator.exe`
7. Compila `example_usage.c` linkeando con `myalloc.lib` → `example.exe`
8. Muestra instrucciones de uso

### `Project v1/example_usage.c` — NUEVO
Programa de ejemplo que:
1. Incluye `myalloc.h`
2. Linkea con `myalloc.lib` (comentado: cómo linkear con `.c` o con `.dll`)
3. Hace 3 allocs, los usa, los libera
4. Comentarios demostrando los 3 niveles de integración

### `Project v1/Makefile` — NUEVO (opcional)
Makefile compatible con GCC/MinGW y MSVC (detectado automáticamente).

### `Project v1/README.md` — ACTUALIZAR
- Agregar sección "Integración como Librería Estática"
- Agregar subsecciones: Nivel A (código fuente), Nivel B (.lib), Nivel C (.dll comentado)
- Incluir comandos de build

---

## Functions

No hay funciones nuevas ni modificadas en `myalloc.c` ni en `myalloc.h`. Solo cambios de preprocesador.

### `mi_print_stats` en `myalloc.h`
- **Antes**: `void mi_print_stats(void);` (siempre disponible)
- **Después**:
```c
#ifdef MYALLOC_DEBUG
void mi_print_stats(void);
#else
#define mi_print_stats() ((void)0)
#endif
```

---

## Classes

`Block_t` — sin cambios.

---

## Dependencies

Ninguna nueva. MSVC `lib.exe` viene con Visual Studio.

---

## Testing

El test suite existente (`main.c` con 20 tests) debe seguir funcionando al linkear contra `myalloc.lib` en vez de compilar `myalloc.c` directamente.

### Verificación:
1. `build.bat` debe ejecutarse sin errores
2. `.lib` generado debe ser un archivo PE válido
3. `allocator.exe` (linkeado con `.lib`) debe pasar 20/20 tests
4. `example.exe` debe ejecutarse sin errores

---

## Implementation Order

1. **Modificar `myalloc.h`**: Envolver `mi_print_stats` en `#ifdef MYALLOC_DEBUG`
2. **Crear `build.bat`**: Script completo de build con MSVC
3. **Crear `example_usage.c`**: Programa ejemplo
4. **Crear `Makefile`**: Para entornos GCC
5. **Ejecutar `build.bat`**: Verificar que todo funciona
6. **Actualizar `README.md`**: Documentación de integración