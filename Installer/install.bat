@echo off
setlocal

:: Request Admin Privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    goto :admin
) else (
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    del "%temp%\getadmin.vbs"
    exit /B
)
:admin

echo ==============================================
echo VxKex for Windows Vista (x64 Installer)
echo ==============================================
echo.

set "SCRIPT_DIR=%~dp0"
set "TARGET_DIR=C:\VxKex"

:: 1. Copy files to C:\VxKex
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
copy /y "%SCRIPT_DIR%*.dll" "%TARGET_DIR%\" >nul
copy /y "%SCRIPT_DIR%VxKexLdr.exe" "%TARGET_DIR%\" >nul 2>&1

:: 2. Copy KexDll.dll and ALL Kx*.dll to System32 (requires 64-bit redirection fix if run from 32-bit)
if exist "%windir%\Sysnative" (
    copy /y "%TARGET_DIR%\KexDll.dll" "%windir%\Sysnative\KexDll.dll" >nul
    copy /y "%TARGET_DIR%\Kx*.dll" "%windir%\Sysnative\" >nul
) else (
    copy /y "%TARGET_DIR%\KexDll.dll" "%windir%\System32\KexDll.dll" >nul
    copy /y "%TARGET_DIR%\Kx*.dll" "%windir%\System32\" >nul
)

:: 3. Register KexDir in Registry (64-bit view)
if exist "%windir%\Sysnative" (
    "%windir%\Sysnative\reg.exe" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f >nul
    "%windir%\Sysnative\reg.exe" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "C:\VxKex\Logs" /f >nul
    "%windir%\Sysnative\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f >nul
    "%windir%\Sysnative\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
) else (
    reg add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f >nul
    reg add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "C:\VxKex\Logs" /f >nul
    reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f >nul
    reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
)
if not exist "C:\VxKex\Logs" mkdir "C:\VxKex\Logs"

:: 4. Register Shell Extension
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InProcServer32" /ve /t REG_SZ /d "%TARGET_DIR%\KexShlEx.dll" /f >nul
reg add "HKCR\CLSID\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f >nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
reg add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /f >nul
reg add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "{9AACA888-A5F5-4C01-852E-8A2005C1D45F}" /f >nul

echo Restarting explorer.exe...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
start explorer.exe

echo.
echo VxKex (x64) has been successfully installed.
echo Press any key to exit.
pause >nul
