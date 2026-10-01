@echo off
setlocal DisableDelayedExpansion
:: Native setup requires the complete package, including VistaSetup.exe.
if not exist "%~dp0VistaSetup.exe" (
    echo VistaSetup.exe is missing. Extract the complete updated package.
    pause
    exit /b 2
)
set "ORIGINAL_SID="
set "MODE="
if "%~1"=="--original-user-sid" (
    if not "%~3"=="" exit /b 87
    set "ORIGINAL_SID=%~2"
    goto :elevation
)
if "%~1"=="--check-install" (
    if not "%~2"=="" exit /b 87
    set "MODE=check-install"
    goto :elevation
)
if not "%~1"=="" exit /b 87
:elevation
"%~dp0VistaSetup.exe" --is-elevated >nul 2>&1
if not errorlevel 1 goto :elevated
if "%MODE%"=="check-install" (
    echo Run the diagnostic check from an elevated command prompt.
    exit /b 5
)
:: Capture the invoking token's SID inside the native helper, before UAC.
"%~dp0VistaSetup.exe" --elevate-installer
exit /b %errorlevel%
:elevated
if defined ORIGINAL_SID goto :ready
for /f "delims=" %%S in ('""%~dp0VistaSetup.exe" --current-user-sid"') do set "ORIGINAL_SID=%%S"
if not defined ORIGINAL_SID exit /b 5
:ready
if defined MODE goto :stage
:menu
cls
echo ========================================================
echo   VxKex for Windows Vista / Server 2008 (x64) Setup
echo ========================================================
echo   [1] Install VxKex
echo   [2] Update VxKex
echo   [3] Reset Configuration
echo   [4] Uninstall VxKex
echo   [5] Exit
set "CHOICE="
set /p CHOICE="Please select an option (1-5): "
if "%CHOICE%"=="1" goto :install
if "%CHOICE%"=="2" goto :update
if "%CHOICE%"=="3" goto :reset
if "%CHOICE%"=="4" goto :uninstall
if "%CHOICE%"=="5" exit /b 0
goto :menu
:install
set "MODE=install"
goto :stage
:update
if not exist "C:\VxKex" (
    echo VxKex is not installed. Choose Install instead.
    pause
    goto :menu
)
set "MODE=install"
goto :stage
:reset
if not exist "%~dp0Reset-VxKex-Config.bat" (
    echo Reset-VxKex-Config.bat is missing.
    pause
    goto :menu
)
call "%~dp0Reset-VxKex-Config.bat"
goto :menu
:uninstall
echo Compatibility settings can be retained for the next installation.
choice /c YNC /n /m "Keep settings? [Y] Keep  [N] Remove  [C] Cancel: "
if errorlevel 3 goto :menu
if not errorlevel 1 goto :menu
if errorlevel 2 goto :remove
set "MODE=uninstall-keep"
goto :stage
:remove
set "MODE=uninstall-remove"
:stage
set "STAGED_PACKAGE="
for /f "delims=" %%S in ('""%~dp0VistaSetup.exe" --prepare-cache "%~dp0.""') do set "STAGED_PACKAGE=%%S"
if not defined STAGED_PACKAGE (
    echo Could not prepare the protected setup workspace. Installation was not changed.
    if not "%MODE%"=="check-install" pause
    exit /b 1
)
if not exist "%STAGED_PACKAGE%\Run-VxKexSetup.cmd" exit /b 2
echo Setup workspace: %STAGED_PACKAGE%
cd /d "%windir%"
if errorlevel 1 exit /b 5
set "VXKEX_SETUP_STAGED=1"
:: Transfer without CALL: the source batch may be replaced or removed by setup.
"%STAGED_PACKAGE%\Run-VxKexSetup.cmd" "%MODE%" "%ORIGINAL_SID%"
exit /b 1
