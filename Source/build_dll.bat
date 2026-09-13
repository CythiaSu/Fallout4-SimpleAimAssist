@echo off
setlocal

set "SOURCE_DIR=%~dp0"
cd /d "%SOURCE_DIR%"

if "%COMMONLIBF4_ROOT%"=="" (
    echo [ERROR] Set COMMONLIBF4_ROOT to your CommonLibF4 checkout.
    exit /b 1
)

where xmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] xmake was not found in PATH.
    exit /b 1
)

echo Building SimpleAimAssist 1.2.8 test package...
xmake build -P . -r -j2
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [DONE] build\windows\x64\release\SimpleAimAssist.dll
