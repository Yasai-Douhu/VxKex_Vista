@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: VxKex Configuration Reset Tool for Windows Vista / Server 2008
:: ============================================================================

:: 1. Request Administrator Privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrative privileges...
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    del "%temp%\getadmin.vbs"
    exit /B
)

cd /d "%~dp0"

echo ========================================================
echo   VxKex Configuration Reset Tool
echo ========================================================
echo.
echo This tool will reset all VxKex global settings and all
echo application compatibility profiles to their default clean state.
echo.
set /p CONFIRM="Are you sure you want to reset all VxKex configuration? (Y/N): "
if /i not "%CONFIRM%"=="Y" exit /b 0

echo.
echo [*] Applying clean registry settings from Reset-VxKex-Config.reg...
reg import "%~dp0Reset-VxKex-Config.reg" >nul 2>&1

echo [*] Scanning and cleaning all custom application IFEO profiles...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ifeoPaths = @('HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options', 'HKLM:\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Image File Execution Options');" ^
    "foreach ($base in $ifeoPaths) {" ^
    "    if (Test-Path $base) {" ^
    "        Get-ChildItem $base | ForEach-Object {" ^
    "            $k = $_;" ^
    "            if ($k.PSChildName -ne '{VxKexPropagationVirtualKey}') {" ^
    "                $p = Get-ItemProperty $k.PSPath;" ^
    "                foreach ($prop in $p.PSObject.Properties) {" ^
    "                    if ($prop.Name -like 'KEX_*') { Remove-ItemProperty -Path $k.PSPath -Name $prop.Name -Force -ErrorAction SilentlyContinue }" ^
    "                    if ($prop.Name -eq 'Debugger' -and $prop.Value -like '*VistaRun.exe*') { Remove-ItemProperty -Path $k.PSPath -Name 'Debugger' -Force -ErrorAction SilentlyContinue }" ^
    "                    if ($prop.Name -eq 'KEX_VistaDebugger') { Remove-ItemProperty -Path $k.PSPath -Name 'KEX_VistaDebugger' -Force -ErrorAction SilentlyContinue }" ^
    "                    if ($prop.Name -eq 'VerifierDlls' -and $prop.Value -like '*kexdll.dll*') { Remove-ItemProperty -Path $k.PSPath -Name 'VerifierDlls' -Force -ErrorAction SilentlyContinue }" ^
    "                }" ^
    "            }" ^
    "        }" ^
    "    }" ^
    "}"

echo [*] Restarting explorer.exe to apply changes...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
start explorer.exe

echo.
echo ========================================================
echo   VxKex configuration has been successfully reset!
echo ========================================================
echo.
pause
exit /B
