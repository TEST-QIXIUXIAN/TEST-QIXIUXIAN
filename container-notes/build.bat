@echo off
rem Build container-notes.exe on Windows. Needs either MinGW-w64 gcc or MSVC (cl) on PATH.
cd /d "%~dp0"

where gcc >nul 2>nul
if %errorlevel%==0 (
    gcc -O2 -static -o container-notes.exe container_notes.c -lws2_32 -lshell32
    if errorlevel 1 goto failed
    goto done
)

where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /O2 /utf-8 container_notes.c ws2_32.lib shell32.lib /Fe:container-notes.exe
    if errorlevel 1 goto failed
    del /q container_notes.obj 2>nul
    goto done
)

echo No C compiler found. Install MinGW-w64 (gcc) or Visual Studio Build Tools (cl), then run again.
echo For MSVC, run this from "Developer Command Prompt for VS".
goto end

:failed
echo Build failed.
goto end

:done
echo Built container-notes.exe - double-click it to start.

:end
pause
