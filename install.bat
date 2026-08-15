@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: VxKex for Windows Vista / Server 2008 (x64) Installer & Uninstaller
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

:: Ensure current working directory is the batch file's folder
cd /d "%~dp0"
set "SCRIPT_DIR=%~dp0"
set "TARGET_DIR=C:\VxKex"
set "CLSID={9AACA888-A5F5-4C01-852E-8A2005C1D45F}"

:: Determine 64-bit System32 directory and Reg command
if exist "%windir%\Sysnative\cmd.exe" (
    set "SYS32_DIR=%windir%\Sysnative"
    set "REG_CMD=%windir%\Sysnative\reg.exe"
) else (
    set "SYS32_DIR=%windir%\System32"
    set "REG_CMD=reg.exe"
)

:menu
cls
echo ========================================================
echo   VxKex for Windows Vista / Server 2008 (x64) Setup
echo ========================================================
echo.
echo   [1] Install VxKex
echo   [2] Uninstall VxKex
echo   [3] Exit
echo.
set /p CHOICE="Please select an option (1-3): "

if "%CHOICE%"=="1" goto :install
if "%CHOICE%"=="2" goto :uninstall
if "%CHOICE%"=="3" exit /B
goto :menu

:install
cls
echo ========================================================
echo   Installing VxKex...
echo ========================================================
echo.

:: 1. Create Target Directory & Copy files
echo [*] Copying VxKex files to %TARGET_DIR%...
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
copy /y "%SCRIPT_DIR%*.dll" "%TARGET_DIR%\" >nul
if exist "%SCRIPT_DIR%VxKexLdr.exe" copy /y "%SCRIPT_DIR%VxKexLdr.exe" "%TARGET_DIR%\" >nul
if exist "%SCRIPT_DIR%KexCfg.exe" copy /y "%SCRIPT_DIR%KexCfg.exe" "%TARGET_DIR%\" >nul
copy /y "%~f0" "%TARGET_DIR%\install.bat" >nul

:: 2. Copy KexDll.dll and Kx*.dll to System32
echo [*] Deploying compatibility libraries to System32...
copy /y "%TARGET_DIR%\KexDll.dll" "%SYS32_DIR%\KexDll.dll" >nul
copy /y "%TARGET_DIR%\Kx*.dll" "%SYS32_DIR%\" >nul

:: 3. Configure VxKex Registry Entries
echo [*] Registering VxKex paths and configuration...
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
if not exist "%TARGET_DIR%\Logs" mkdir "%TARGET_DIR%\Logs"

:: 4. Register Shell Extension (KexShlEx.dll)
echo [*] Registering Shell Extension...
"%REG_CMD%" add "HKCR\CLSID\%CLSID%" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
"%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /ve /t REG_SZ /d "%TARGET_DIR%\KexShlEx.dll" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /ve /t REG_SZ /d "%TARGET_DIR%\KexShlEx.dll" /f >nul
"%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "VxKex Property Sheet Handler" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
"%REG_CMD%" add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f >nul
"%REG_CMD%" add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f >nul

:: 5. Restart Explorer
echo [*] Restarting explorer.exe to apply changes...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
start explorer.exe

echo.
echo ========================================================
echo   VxKex (x64) has been successfully installed!
echo ========================================================
echo.
pause
exit /B

:uninstall
cls
echo ========================================================
echo   Uninstalling VxKex...
echo ========================================================
echo.

:: 1. Unregister Shell Extension
echo [*] Unregistering Shell Extension...
"%REG_CMD%" delete "HKCR\CLSID\%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKCR\CLSID\%CLSID%" /f >nul 2>&1
"%REG_CMD%" delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /f >nul 2>&1
"%REG_CMD%" delete "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /f >nul 2>&1
"%REG_CMD%" delete "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /f >nul 2>&1

:: 2. Remove Registry Configuration
echo [*] Removing VxKex registry entries...
"%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft\VxKex" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft\VxKex" /f >nul 2>&1
"%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft\VxKexLdr" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft\VxKexLdr" /f >nul 2>&1
"%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKLM\SOFTWARE\VXsoft" /f >nul 2>&1
"%REG_CMD%" delete "HKCU\Software\VXsoft" /f >nul 2>&1
"%REG_CMD%" delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /f /reg:64 >nul 2>&1 || "%REG_CMD%" delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /f >nul 2>&1

:: 3. Stop explorer.exe to unlock files
echo [*] Stopping explorer.exe to release DLL handles...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul

:: 4. Delete deployed DLLs from System32
echo [*] Removing compatibility libraries from System32...
del /f /q "%SYS32_DIR%\KexDll.dll" >nul 2>&1
del /f /q "%SYS32_DIR%\Kx*.dll" >nul 2>&1

:: 5. Remove Target Directory
echo [*] Removing %TARGET_DIR%...
if exist "%TARGET_DIR%" (
    rmdir /s /q "%TARGET_DIR%" >nul 2>&1
)

:: 6. Restart explorer.exe
echo [*] Restarting explorer.exe...
start explorer.exe

echo.
echo ========================================================
echo   VxKex (x64) has been successfully uninstalled.
echo ========================================================
echo.
pause
exit /B
