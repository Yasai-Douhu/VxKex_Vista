@echo off
setlocal DisableDelayedExpansion
if not "%VXKEX_SETUP_STAGED%"=="1" (
    echo Start install.bat to prepare the external setup workspace.
    exit /b 87
)
set "MODE=%~1"
set "ORIGINAL_SID=%~2"
if not "%~3"=="" exit /b 87
if not defined ORIGINAL_SID exit /b 87
set "CHECK_ONLY=0"
if "%MODE%"=="check-install" set "CHECK_ONLY=1"
if "%MODE%"=="check-uninstall-keep" set "CHECK_ONLY=1"
if "%MODE%"=="check-uninstall-remove" set "CHECK_ONLY=1"
if "%MODE%"=="install" goto :validated
if "%MODE%"=="uninstall-keep" goto :validated
if "%MODE%"=="uninstall-remove" goto :validated
if "%CHECK_ONLY%"=="1" goto :validated
exit /b 87
:validated
cd /d "%windir%"
if errorlevel 1 exit /b 5
"%~dp0VistaSetup.exe" --is-elevated
if errorlevel 1 exit /b 5
set "RESTART_EXPLORER=0"
if "%CHECK_ONLY%"=="1" goto :execute
tasklist /fi "imagename eq explorer.exe" /nh | findstr /i /l "explorer.exe" >nul
if errorlevel 1 goto :execute
echo Close VxKex applications before continuing. Setup will restart Explorer.
set "RESTART_EXPLORER=1"
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
:execute
if "%MODE%"=="install" goto :install
if "%MODE%"=="check-install" goto :install
"%~dp0VistaSetup.exe" --%MODE% --user-sid "%ORIGINAL_SID%"
goto :result
:install
"%~dp0VistaSetup.exe" --%MODE% "%~dp0." --user-sid "%ORIGINAL_SID%"
:result
set "RESULT=%errorlevel%"
if "%RESTART_EXPLORER%"=="1" start "" "%windir%\explorer.exe"
if not "%RESULT%"=="0" (
    echo Setup failed with code %RESULT%. See the stage and rollback messages above.
    if "%CHECK_ONLY%"=="0" pause
    goto :finish
)
if "%CHECK_ONLY%"=="1" (
    echo Setup check completed. Installation changes were rolled back.
    goto :finish
)
echo Setup completed successfully.
pause
:finish
"%~dp0VistaSetup.exe" --complete-cache
if errorlevel 1 echo Setup cache retained for a later cleanup attempt.
exit /b %RESULT%
