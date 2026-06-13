@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  myalloc - Build Script
echo  Static Library (.lib) + Examples
echo ========================================
echo.

call "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
)
if errorlevel 1 (
    echo [ERROR] No se encontro Visual Studio. Abortando.
    exit /b 1
)

set CFLAGS=/std:c11 /nologo /W3
set SRCDIR=.
set OUTDIR=.

echo [1/5] Compilando myalloc.c (release)...
cl.exe %CFLAGS% /c /Fo"%OUTDIR%\myalloc.obj" "%SRCDIR%\myalloc.c"
if errorlevel 1 goto :error

echo [2/5] Compilando myalloc.c (debug)...
cl.exe %CFLAGS% /DMYALLOC_DEBUG /c /Fo"%OUTDIR%\myalloc_dbg.obj" "%SRCDIR%\myalloc.c"
if errorlevel 1 goto :error

echo [3/5] Creando librerias estaticas...
lib.exe /nologo /out:"%OUTDIR%\myalloc.lib" "%OUTDIR%\myalloc.obj"
if errorlevel 1 goto :error

lib.exe /nologo /out:"%OUTDIR%\myalloc_dbg.lib" "%OUTDIR%\myalloc_dbg.obj"
if errorlevel 1 goto :error

echo [4/5] Compilando test suite y ejemplo...
cl.exe %CFLAGS% /DMYALLOC_DEBUG /Fe:"%OUTDIR%\allocator.exe" "%SRCDIR%\main.c" "%OUTDIR%\myalloc_dbg.lib"
if errorlevel 1 goto :error

cl.exe %CFLAGS% /DMYALLOC_DEBUG /Fe:"%OUTDIR%\example.exe" "%SRCDIR%\example_usage.c" "%OUTDIR%\myalloc_dbg.lib"
if errorlevel 1 goto :error

echo [5/5] Ejecutando tests...
echo.
"%OUTDIR%\allocator.exe"
if errorlevel 1 goto :error

echo.
echo ========================================
echo  BUILD EXITOSO
echo ========================================
echo.
echo Archivos generados:
echo   myalloc.lib      - Libreria estatica (release)
echo   myalloc_dbg.lib  - Libreria estatica (debug)
echo   myalloc.obj      - Objeto (release)
echo   myalloc_dbg.obj  - Objeto (debug)
echo   allocator.exe    - Test suite (20 tests)
echo   example.exe      - Programa de ejemplo
echo.
echo Como usar en tu proyecto:
echo   1. Copia myalloc.h y myalloc.lib a tu proyecto
echo   2. En tu codigo: #include "myalloc.h"
echo   3. Al compilar, linkea con myalloc.lib:
echo      cl.exe /Fe:programa.exe programa.c myalloc.lib
echo.
goto :end

:error
echo.
echo [ERROR] El build fallo. Revisa los mensajes arriba.
exit /b 1

:end
endlocal