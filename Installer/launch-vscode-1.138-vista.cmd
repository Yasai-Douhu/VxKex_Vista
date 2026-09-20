@echo off
setlocal
set "CODE=%USERPROFILE%\Downloads\VSCode-win32-x64-1.138.0\Code.exe"
if not "%~1"=="" set "CODE=%~1"
set "RUNNER=C:\VxKex\VistaRun.exe"
if not exist "%RUNNER%" set "RUNNER=%~dp0VistaRun.exe"
if not exist "%CODE%" (
    echo Code.exe was not found. Pass its full path as the first argument.
    pause
    exit /b 1
)
if not exist "%RUNNER%" (
    echo VistaRun.exe was not found. Install the updated VxKex package first.
    pause
    exit /b 1
)
:: Vista cannot provide the sandbox required by this version of Electron.
:: GPU/WebGL and sandbox isolation are disabled. See docs/vscode-1.138-vista.md.
"%RUNNER%" "%CODE%" --no-sandbox --disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled --user-data-dir="%LOCALAPPDATA%\VxKex\VSCode-1.138"
