@echo off
setlocal
net session >nul 2>&1
if errorlevel 1 (
    echo Run this script as administrator after installing VxKex.
    exit /b 1
)
set "REG=reg.exe"
if exist "%windir%\Sysnative\reg.exe" set "REG=%windir%\Sysnative\reg.exe"
set "K=HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\Code.exe"
:: Keep the first pre-configuration snapshot when a Code.exe key exists.
if not exist "%~dp0Code.before-vista.reg" "%REG%" export "%K%" "%~dp0Code.before-vista.reg" >nul 2>&1
"%REG%" add "%K%" /v GlobalFlag /t REG_DWORD /d 256 /f >nul || exit /b 1
"%REG%" add "%K%" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul || exit /b 1
"%REG%" add "%K%" /v VerifierDlls /t REG_SZ /d KexDll.dll /f >nul || exit /b 1
"%REG%" add "%K%" /v KEX_WinVerSpoof /t REG_DWORD /d 4 /f >nul || exit /b 1
"%REG%" add "%K%" /v KEX_StrongVersionSpoof /t REG_DWORD /d 2 /f >nul || exit /b 1
"%REG%" add "%K%" /v KEX_DisableForChild /t REG_DWORD /d 0 /f >nul || exit /b 1
"%REG%" add "%K%" /v KEX_DisableAppSpecific /t REG_DWORD /d 0 /f >nul || exit /b 1
echo VSCode compatibility configuration applied.
