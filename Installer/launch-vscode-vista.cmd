@echo off
setlocal
:: Optional first argument: full path to the extracted x64 Code.exe.
set "CODE=C:\VSCode1702\code$GetDestDir\Code.exe"
if not "%~1"=="" set "CODE=%~1"
if not exist "%CODE%" (
    echo Code.exe was not found. Pass its full path as the first argument.
    pause
    exit /b 1
)
:: Vista VM: disable GPU sandbox and both hardware and SwiftShader 3D paths.
:: This reduces isolation and disables WebGL. See docs/vscode-vista.md.
start "" "%CODE%" --disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled
