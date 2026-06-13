# Asignador de Memoria — `mi_malloc` / `mi_free`

Librería C que gestiona un bloque estático de 1 MB usando lista enlazada de metadatos, First-Fit, coalescing, spin-lock, alineación a 16 bytes y guardrails 0xDEADBEEF.

---

## Layout de un Bloque Ocupado

```mermaid
graph LR
    subgraph HEAP[Heap 1 MB]
        H[Header 32 B] --> PD[Pad 16 B]
        PD --> D[Datos]
        D --> G[Guardrail 16 B]
        G --> H2[Header 32 B ...]
    end
```

```
0x00  Block_t.size            8 B
0x08  Block_t.free            4 B
0x0C  Block_t.magic=0xDEADBEEF 4 B
0x10  Block_t.next            8 B
0x18  Block_t.prev            8 B
                           ───────── 32 B (header)
0x20  Padding                 16 B
                           ───────── 48 B hasta datos
0x30  Datos del usuario       size
0x30+size  Guardrail post     16 B  = 0xDEADBEEF x 4
                           ───────── size + 64 B overhead
```

## API

```c
void mi_init(void);
void *mi_malloc(size_t size);
void mi_free(void *ptr);
void mi_print_stats(void);   // solo con -DMYALLOC_DEBUG
```

## Flujo mi_malloc

```mermaid
flowchart TD
    START[mi_malloc size] --> Z{size==0}
    Z -->|Si| RET0[return NULL]
    Z -->|No| ALIGN[Alinear a 16]
    ALIGN --> LOCK[lock_acquire]
    LOCK --> SEARCH[Buscar bloque free<br/>con size suficiente]
    SEARCH -->|No encuentra| UNLOCK[lock_release]
    UNLOCK --> RETNULL[return NULL]
    SEARCH -->|Encuentra| CHECK{remaining mayor<br/>que BLOCK_OVERHEAD}
    CHECK -->|Si| SPLIT[Crear nuevo bloque<br/>con espacio sobrante]
    SPLIT --> FIX[Ajustar punteros<br/>next y prev]
    CHECK -->|No| FIX
    FIX --> SETUP[curr.size = size<br/>curr.free = 0<br/>curr.magic = 0xDEADBEEF]
    SETUP --> GUARD[write_guard<br/>post-datos = 0xDEADBEEF x 4]
    GUARD --> UNLK[lock_release]
    UNLK --> RET[return ptr a datos<br/>header + 48 B]
```

## Flujo mi_free

```mermaid
flowchart TD
    START[mi_free ptr] --> N{ptr==NULL}
    N -->|Si| RET0[return]
    N -->|No| LOCK[lock_acquire]
    LOCK --> BLOCK[block = ptr - 48]
    BLOCK --> DF{magic igual a<br/>0xBAADF00D}
    DF -->|Si| RET1[lock_release<br/>return silencioso]
    DF -->|No| UF{magic distinto de<br/>0xDEADBEEF}
    UF -->|Si| ERRU[ERROR underflow<br/>header corrupto]
    ERRU --> RET2[lock_release<br/>return]
    UF -->|No| OVF{post-guardrail<br/>intacto}
    OVF -->|No| ERRO[ERROR overflow]
    ERRO --> RET3[lock_release<br/>return]
    OVF -->|Si| FR{block.free}
    FR -->|Si| RET4[lock_release<br/>return]
    FR -->|No| SET[block.free = 1<br/>block.magic = 0xBAADF00D]
    SET --> COAN{next free}
    COAN -->|Si| COANEXT[Coalesce con next]
    COANEXT --> COAP
    COAN -->|No| COAP{prev free}
    COAP -->|Si| COAPREV[Coalesce con prev]
    COAP -->|No| DONE
    COAPREV --> DONE[lock_release]
    DONE --> RET5[return]
```

## Thread-Safety

```mermaid
sequenceDiagram
    participant T1 as Thread 1
    participant L as Spin-Lock
    participant T2 as Thread 2

    T1->>L: InterlockedExchange(1)
    L-->>T1: 0
    Note over T1: lock adquirido
    T2->>L: InterlockedExchange(1)
    L-->>T2: 1
    Note over T2: espera + mm_pause
    T1->>L: InterlockedExchange(0)
    Note over L: lock liberado
    T2->>L: InterlockedExchange(1)
    L-->>T2: 0
    Note over T2: lock adquirido
```

## Coalescing

```mermaid
stateDiagram-v2
    state Antes {
        U0: USED 112 B
        U1: USED 208 B
        U2: USED 64 B
        F3: FREE 1 MB
    }
    U0 --> U1
    U1 --> U2
    U2 --> F3

    state DespuesFreeP2 {
        U0b: USED 112 B
        F1b: FREE 208 B
        U2b: USED 64 B
        F3b: FREE 1 MB
    }
    U0b --> F1b
    F1b --> U2b
    U2b --> F3b

    state DespuesFreeP1 {
        F0c: FREE 384 B
        U2c: USED 64 B
        F3c: FREE 1 MB
    }
    F0c --> U2c
    U2c --> F3c

    Antes --> DespuesFreeP2: mi_free(p2)
    DespuesFreeP2 --> DespuesFreeP1: mi_free(p1) + coalescing
```

## Deteccion de Errores

| Escenario | Deteccion | Output |
|-----------|-----------|--------|
| Buffer overflow | Post-guardrail corrupto | `ERROR: Buffer overflow detectado` |
| Buffer underflow | Header magic cambiado | `ERROR: Buffer underflow... magic=0xFFFFFFFF` |
| Double-free | magic = 0xBAADF00D | Silencioso, no-op |

## Integracion como Libreria

Tres niveles de integracion:

### Nivel A — Codigo Fuente
Compilas `myalloc.c` junto con tu programa. Simple pero poco escalable.
```bash
cl.exe /std:c11 /Fe:programa.exe programa.c myalloc.c
```

### Nivel B — Libreria Estatica (.lib)  ← RECOMENDADO
Compilas `myalloc.c` una vez y generas un `.lib`. Tu programa solo necesita `myalloc.h` + `myalloc.lib`.
```bash
# Generar la libreria
cl.exe /std:c11 /c myalloc.c
lib.exe /out:myalloc.lib myalloc.obj

# Usarla en tu programa
cl.exe /std:c11 /Fe:programa.exe programa.c myalloc.lib
```

Ventajas: un solo `.exe` autocontenido, compilacion rapida, distribucion simple.

### Nivel C — Libreria Dinamica (.dll)
`myalloc.dll` se carga en RAM cuando el sistema operativo lo pide. Si mejoras el algoritmo, solo reemplazas el `.dll`.
```bash
cl.exe /std:c11 /LD /Fe:myalloc.dll myalloc.c
cl.exe /std:c11 /Fe:programa.exe programa.c myalloc.lib
```

### Build Script
```bash
cd "Project v1"
build.bat
```

Genera:
- `myalloc.lib` — libreria estatica (release)
- `myalloc_dbg.lib` — libreria estatica (debug, con `mi_print_stats`)
- `allocator.exe` — test suite (20 tests)
- `example.exe` — programa de ejemplo

## Tests

| Test | Descripcion | Estado |
|------|-------------|--------|
| 1 | Asignaciones + alineacion 16 B | OK |
| 2 | Coalescing | OK |
| 3 | Fragmentacion + First-Fit | OK |
| 4 | Large alloc 512KB + fallo | OK |
| 5 | Free all | OK |
| 6 | Double-free | OK |
| 7 | Buffer overflow detection | OK |
| 8 | Header corruption detection | OK |

```bash
cd "Project v1"
build.bat           # Build completo + tests
.\allocator.exe     # 20/20 tests
.\example.exe       # Ejemplo de integracion