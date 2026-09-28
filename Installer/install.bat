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
set "X86_DLLS=KexDll KxBase KxNt KxAdvapi KxCom KxCrt KxCryp KxDw KxDx KxMi KxNet KxUia KxUser"
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
echo   [2] Update VxKex
echo   [3] Uninstall VxKex
echo   [4] Exit
echo.
set /p CHOICE="Please select an option (1-4): "

if "%CHOICE%"=="1" goto :install
if "%CHOICE%"=="2" goto :update
if "%CHOICE%"=="3" goto :uninstall
if "%CHOICE%"=="4" exit /B
goto :menu

:update
cls
echo ========================================================
echo   Updating VxKex...
echo ========================================================
echo.

:: 1. Check if VxKex is installed
if not exist "%TARGET_DIR%" (
    echo VxKex is not currently installed at %TARGET_DIR%.
    echo Please select [1] Install VxKex to perform a fresh installation.
    echo.
    pause
    goto :menu
)

:: Verify all components are present in the package
for %%D in (%X86_DLLS%) do (
    if not exist "%SCRIPT_DIR%Kex32\%%D.dll" (
        echo Missing x86 component: Kex32\%%D.dll. Extract the complete package.
        pause
        exit /b 1
    )
)
if not exist "%SCRIPT_DIR%KexCfg.exe" (
    echo KexCfg.exe is missing. Extract the complete release before updating.
    pause
    exit /b 1
)

:: 2. Stop Explorer to unlock files
echo [*] Stopping explorer.exe to unlock compatibility libraries...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul

:: 3. Update VxKex files in target directory
echo [*] Updating VxKex files in %TARGET_DIR%...
if exist "%TARGET_DIR%\KexShlEx.dll" (
    if exist "%TARGET_DIR%\KexShlEx.previous.dll" del /q "%TARGET_DIR%\KexShlEx.previous.dll" >nul 2>&1
    move /y "%TARGET_DIR%\KexShlEx.dll" "%TARGET_DIR%\KexShlEx.previous.dll" >nul
)
copy /y "%SCRIPT_DIR%*.dll" "%TARGET_DIR%\" >nul
if errorlevel 1 (
    echo Failed to copy compatibility libraries. Update stopped.
    start explorer.exe
    pause
    exit /b 1
)
if not exist "%TARGET_DIR%\Kex64" mkdir "%TARGET_DIR%\Kex64"
copy /y "%SCRIPT_DIR%Kex64\*.dll" "%TARGET_DIR%\Kex64\" >nul
if errorlevel 1 (
    echo Failed to copy Kex64 dependencies. Update stopped.
    start explorer.exe
    pause
    exit /b 1
)
if exist "%SCRIPT_DIR%VxKexLdr.exe" copy /y "%SCRIPT_DIR%VxKexLdr.exe" "%TARGET_DIR%\" >nul
copy /y "%SCRIPT_DIR%VistaRun.exe" "%TARGET_DIR%\" >nul
copy /y "%SCRIPT_DIR%KexCfg.exe" "%TARGET_DIR%\" >nul
copy /y "%~f0" "%TARGET_DIR%\install.bat" >nul
if exist "%SCRIPT_DIR%configure-vscode-vista.cmd" copy /y "%SCRIPT_DIR%configure-vscode-vista.cmd" "%TARGET_DIR%\" >nul
if exist "%SCRIPT_DIR%launch-vscode-vista.cmd" copy /y "%SCRIPT_DIR%launch-vscode-vista.cmd" "%TARGET_DIR%\" >nul
if exist "%SCRIPT_DIR%launch-vscode-1.138-vista.cmd" copy /y "%SCRIPT_DIR%launch-vscode-1.138-vista.cmd" "%TARGET_DIR%\" >nul

:: 4. Update deployed DLLs in System32
echo [*] Updating compatibility libraries in System32...
copy /y "%TARGET_DIR%\KexDll.dll" "%SYS32_DIR%\KexDll.dll" >nul
copy /y "%TARGET_DIR%\Kx*.dll" "%SYS32_DIR%\" >nul

:: 5. Update x86 components in Kex32 and SysWOW64
echo [*] Updating x86 compatibility libraries...
if not exist "%TARGET_DIR%\Kex32" mkdir "%TARGET_DIR%\Kex32"
for %%D in (%X86_DLLS%) do (
    copy /y "%SCRIPT_DIR%Kex32\%%D.dll" "%TARGET_DIR%\Kex32\%%D.dll" >nul
    copy /y "%SCRIPT_DIR%Kex32\%%D.dll" "%windir%\SysWOW64\%%D.dll" >nul
)

:: 6. Refresh core configuration & version while preserving application settings
echo [*] Updating VxKex version and registry configuration...
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v InstalledVersion /t REG_DWORD /d 2147485875 /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul
"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d KexDll.dll /f >nul
"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul

:: Re-register Shell Extension if needed
"%REG_CMD%" add "HKCR\CLSID\%CLSID%" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%" /ve /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
"%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /ve /t REG_SZ /d "%TARGET_DIR%\KexShlEx.dll" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /ve /t REG_SZ /d "%TARGET_DIR%\KexShlEx.dll" /f >nul
"%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\CLSID\%CLSID%\InProcServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "VxKex Property Sheet Handler" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "VxKex Property Sheet Handler" /f >nul
"%REG_CMD%" add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\exefile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f >nul
"%REG_CMD%" add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKCR\lnkfile\shellex\PropertySheetHandlers\VxKex" /ve /t REG_SZ /d "%CLSID%" /f >nul

:: 7. Restart Explorer
echo [*] Restarting explorer.exe to apply changes...
start explorer.exe

echo.
echo ========================================================
echo   VxKex (x64) has been successfully updated!
echo ========================================================
echo.
pause
exit /B

:install
cls
echo ========================================================
echo   Installing VxKex...
echo ========================================================
echo.

:: 1. Create Target Directory & Copy files
for %%D in (%X86_DLLS%) do (
    if not exist "%SCRIPT_DIR%Kex32\%%D.dll" (
        echo Missing x86 component: Kex32\%%D.dll. Extract the complete package.
        pause
        exit /b 1
    )
)
if not exist "%SCRIPT_DIR%KexCfg.exe" (
    echo KexCfg.exe is missing. Extract the complete release before installing.
    pause
    exit /b 1
)
echo [*] Copying VxKex files to %TARGET_DIR%...
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
:: Explorer may still have the previous shell extension mapped. Rename it
:: before copying, then the existing Explorer restart loads the new DLL.
if exist "%TARGET_DIR%\KexShlEx.dll" (
    if exist "%TARGET_DIR%\KexShlEx.previous.dll" del /q "%TARGET_DIR%\KexShlEx.previous.dll" >nul 2>&1
    move /y "%TARGET_DIR%\KexShlEx.dll" "%TARGET_DIR%\KexShlEx.previous.dll" >nul
    if errorlevel 1 (
        echo Close open property dialogs and restart Explorer before retrying.
        pause
        exit /b 1
    )
)
copy /y "%SCRIPT_DIR%*.dll" "%TARGET_DIR%\" >nul
if errorlevel 1 (
    echo Failed to copy compatibility libraries. Installation stopped.
    pause
    exit /b 1
)
:: DWrite replacement must remain under KexDir so its imports are rewritten.
if not exist "%TARGET_DIR%\Kex64" mkdir "%TARGET_DIR%\Kex64"
copy /y "%SCRIPT_DIR%Kex64\*.dll" "%TARGET_DIR%\Kex64\" >nul
if errorlevel 1 (
    echo Failed to copy Kex64 dependencies. Installation stopped.
    pause
    exit /b 1
)
if exist "%SCRIPT_DIR%VxKexLdr.exe" copy /y "%SCRIPT_DIR%VxKexLdr.exe" "%TARGET_DIR%\" >nul
copy /y "%SCRIPT_DIR%VistaRun.exe" "%TARGET_DIR%\" >nul
if errorlevel 1 (
    echo Failed to install VistaRun.exe. Installation stopped.
    pause
    exit /b 1
)
copy /y "%SCRIPT_DIR%KexCfg.exe" "%TARGET_DIR%\" >nul
if errorlevel 1 (
    echo Failed to install KexCfg.exe. Installation stopped.
    pause
    exit /b 1
)
copy /y "%~f0" "%TARGET_DIR%\install.bat" >nul

:: 2. Copy KexDll.dll and Kx*.dll to System32
echo [*] Deploying compatibility libraries to System32...
copy /y "%TARGET_DIR%\KexDll.dll" "%SYS32_DIR%\KexDll.dll" >nul
copy /y "%TARGET_DIR%\Kx*.dll" "%SYS32_DIR%\" >nul

:: 3. Configure VxKex Registry Entries
:: A WOW64 verifier provider must be x86 and discoverable in SysWOW64.
if not exist "%TARGET_DIR%\Kex32" mkdir "%TARGET_DIR%\Kex32"
for %%D in (%X86_DLLS%) do (
    copy /y "%SCRIPT_DIR%Kex32\%%D.dll" "%TARGET_DIR%\Kex32\%%D.dll" >nul
    if errorlevel 1 (
        echo Failed to install x86 %%D.dll. Close 32-bit applications and retry.
        pause
        exit /b 1
    )
    copy /y "%SCRIPT_DIR%Kex32\%%D.dll" "%windir%\SysWOW64\%%D.dll" >nul
    if errorlevel 1 (
        echo Failed to deploy x86 %%D.dll to SysWOW64.
        pause
        exit /b 1
    )
)
echo [*] Registering VxKex paths and configuration...
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v InstalledVersion /t REG_DWORD /d 2147485875 /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v KexDir /t REG_SZ /d "%TARGET_DIR%" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\VXsoft\VxKex" /v LogDir /t REG_SZ /d "%TARGET_DIR%\Logs" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d "KexDll.dll" /f >nul
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f /reg:64 >nul 2>&1 || "%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
if not exist "%TARGET_DIR%\Logs" mkdir "%TARGET_DIR%\Logs"
:: Custom verifier provider only; zero enables Vista's default verifier checks.
"%REG_CMD%" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul

"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierDlls /t REG_SZ /d KexDll.dll /f >nul
if errorlevel 1 exit /b 1
"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v GlobalFlag /t REG_DWORD /d 256 /f >nul
if errorlevel 1 exit /b 1
"%windir%\SysWOW64\reg.exe" add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul
if errorlevel 1 exit /b 1

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

:: Remove owned IFEO launch commands before removing their executable.
if not exist "%TARGET_DIR%\VistaRun.exe" (
    echo VistaRun.exe is missing. Restore it before uninstalling.
    pause
    exit /b 1
)
"%TARGET_DIR%\VistaRun.exe" --remove-launchers
if errorlevel 1 (
    echo Could not remove VxKex launch commands. Uninstallation stopped.
    pause
    exit /b 1
)

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
"%windir%\SysWOW64\reg.exe" delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\{VxKexPropagationVirtualKey}" /f >nul 2>&1
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
for %%D in (%X86_DLLS%) do del /f /q "%windir%\SysWOW64\%%D.dll" >nul 2>&1
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
