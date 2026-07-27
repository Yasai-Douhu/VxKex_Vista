@echo off
rem #############################################################################
rem #
rem # VxKex-Vista Deployment Script
rem # Target: Windows Vista SP2 x64
rem #
rem # This script deploys VxKex-Vista DLLs and configures IFEO (Image File
rem # Execution Options) to automatically load KexDll.dll for specified
rem # executables.
rem #
rem # Usage:
rem #   deploy_to_vista.bat [Install|Uninstall] [TargetDir] [ExeName]
rem #
rem # Examples:
rem #   deploy_to_vista.bat install "C:\VxKex" notepad.exe
rem #   deploy_to_vista.bat uninstall "C:\VxKex" notepad.exe
rem #   deploy_to_vista.bat install-all "C:\VxKex"  (deploy to all target apps)
rem #   deploy_to_vista.bat register                 (register shell extension only)
rem #   deploy_to_vista.bat unregister               (unregister shell extension only)
rem #
rem #############################################################################

setlocal EnableDelayedExpansion

rem =============================================================================
rem Configuration
rem =============================================================================
set "SCRIPT_DIR=%~dp0"
set "VISTA_DLLS_DIR=%SCRIPT_DIR%VistaDLLs"
set "DEFAULT_TARGET_DIR=C:\VxKex"
set "KEXDLL_NAME=KexDll.dll"
set "KEXSHLEXE_NAME=KexShlEx.dll"
set "KXNT_NAME=KxNt.dll"
set "KXBASE_NAME=KxBase.dll"
set "VXKEXLDR_NAME=VxKexLdr.exe"

rem Target applications for IFEO configuration (space-separated)
set "TARGET_APPS=notepad.exe calc.exe explorer.exe"

rem CLSID for KexShlEx shell extension
rem {9AACA888-A5F5-4C01-852E-8A2005C1D45F}
set "KEXSHLEX_CLSID={9AACA888-A5F5-4C01-852E-8A2005C1D45F}"

rem =============================================================================
rem Functions
rem =============================================================================

:Log
echo [%TIME%] %~1
goto :eof

:CheckAdmin
rem Check if script is running with administrative privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] This script must be run as Administrator.
    echo         Right-click and select "Run as administrator".
    pause
    exit /b 1
)
goto :eof

:CopyDlls
rem Copy VxKex DLLs to target directory
set "TargetDir=%~1"

if not exist "%TargetDir%" (
    echo [INFO] Creating target directory: %TargetDir%
    mkdir "%TargetDir%"
)

echo [INFO] Copying VxKex DLLs to %TargetDir%...

rem Copy DLLs
if exist "%VISTA_DLLS_DIR%\%KEXDLL_NAME%" (
    copy /y "%VISTA_DLLS_DIR%\%KEXDLL_NAME%" "%TargetDir%\" >nul
    echo   - %KEXDLL_NAME% copied
) else (
    echo [WARNING] %KEXDLL_NAME% not found in VistaDLLs/
)

if exist "%VISTA_DLLS_DIR%\%KEXSHLEXE_NAME%" (
    copy /y "%VISTA_DLLS_DIR%\%KEXSHLEXE_NAME%" "%TargetDir%\" >nul
    echo   - %KEXSHLEXE_NAME% copied
) else (
    echo [WARNING] %KEXSHLEXE_NAME% not found in VistaDLLs/
)

if exist "%VISTA_DLLS_DIR%\%KXNT_NAME%" (
    copy /y "%VISTA_DLLS_DIR%\%KXNT_NAME%" "%TargetDir%\" >nul
    echo   - %KXNT_NAME% copied
) else (
    echo [WARNING] %KXNT_NAME% not found in VistaDLLs/
)

if exist "%VISTA_DLLS_DIR%\%KXBASE_NAME%" (
    copy /y "%VISTA_DLLS_DIR%\%KXBASE_NAME%" "%TargetDir%\" >nul
    echo   - %KXBASE_NAME% copied
) else (
    echo [WARNING] %KXBASE_NAME% not found in VistaDLLs/
)

rem Check for VxKexLdr.exe
if exist "%SCRIPT_DIR%VxKex_Vista\x64\Release\VxKexLdr\VxKexLdr.exe" (
    copy /y "%SCRIPT_DIR%VxKex_Vista\x64\Release\VxKexLdr\VxKexLdr.exe" "%TargetDir%\" >nul
    echo   - %VXKEXLDR_NAME% copied
) else if exist "%VISTA_DLLS_DIR%\%VXKEXLDR_NAME%" (
    copy /y "%VISTA_DLLS_DIR%\%VXKEXLDR_NAME%" "%TargetDir%\" >nul
    echo   - %VXKEXLDR_NAME% copied
) else (
    echo [INFO] %VXKEXLDR_NAME% not found (optional)
)

echo [INFO] DLL copy completed.
goto :eof

:ApplyIfeo
rem Configure IFEO for a specific executable
rem Arguments: TargetDir, ExeName
set "TargetDir=%~1"
set "ExeName=%~2"

if not defined ExeName (
    echo [ERROR] Executable name not specified.
    goto :eof
)

echo [INFO] Configuring IFEO for %ExeName%...

rem Get absolute path to KexDll.dll
set "KexDllPath=%TargetDir%\%KEXDLL_NAME%"

if not exist "%KexDllPath%" (
    echo [ERROR] %KexDllPath% does not exist. Please copy DLLs first.
    goto :eof
)

rem Calculate relative path from System32
for %%i in ("%KexDllPath%") do set "RelativePath=%%~fi"

rem Set IFEO registry keys
set "IfeoKey=HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%ExeName%"

echo [INFO] Setting VerifierDlls...
reg add "%IfeoKey%" /v VerifierDlls /t REG_MULTI_SZ /d "%RelativePath%" /f >nul
if %errorlevel% equ 0 (
    echo   - VerifierDlls set successfully
) else (
    echo [ERROR] Failed to set VerifierDlls
)

echo [INFO] Setting GlobalFlag (0x80000000 = FLG_APPLICATION_VERIFIER)...
reg add "%IfeoKey%" /v GlobalFlag /t REG_DWORD /d 2147483648 /f >nul
if %errorlevel% equ 0 (
    echo   - GlobalFlag set successfully
) else (
    echo [ERROR] Failed to set GlobalFlag
)

echo [INFO] Setting VerifierFlags (0x80000000)...
reg add "%IfeoKey%" /v VerifierFlags /t REG_DWORD /d 2147483648 /f >nul
if %errorlevel% equ 0 (
    echo   - VerifierFlags set successfully
) else (
    echo [ERROR] Failed to set VerifierFlags
)

goto :eof

:RemoveIfeo
rem Remove IFEO configuration for a specific executable
set "ExeName=%~1"

if not defined ExeName (
    echo [ERROR] Executable name not specified.
    goto :eof
)

echo [INFO] Removing IFEO for %ExeName%...

set "IfeoKey=HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%ExeName%"

rem Remove specific VxKex-related values
reg delete "%IfeoKey%" /v VerifierDlls /f >nul 2>&1
reg delete "%IfeoKey%" /v GlobalFlag /f >nul 2>&1
reg delete "%IfeoKey%" /v VerifierFlags /f >nul 2>&1

echo [INFO] IFEO removal completed for %ExeName%.
goto :eof

:RegisterShellExt
rem Register KexShlEx.dll as a COM shell extension
set "DllPath=%~1"

if not defined DllPath (
    set "DllPath=%DEFAULT_TARGET_DIR%\%KEXSHLEXE_NAME%"
)

if not exist "%DllPath%" (
    echo [ERROR] %DllPath% does not exist. Please deploy DLLs first.
    goto :eof
)

echo [INFO] Registering KexShlEx.dll...
echo [INFO] CLSID: %KEXSHLEXE_CLSID%

rem Register the DLL using regsvr32
regsvr32 /s "%DllPath%"
if %errorlevel% equ 0 (
    echo [INFO] KexShlEx.dll registered successfully.
) else (
    echo [ERROR] Failed to register KexShlEx.dll (error code: %errorlevel%)
    echo [INFO] You may need to register manually:
    echo        regsvr32 "%DllPath%"
)

goto :eof

:UnregisterShellExt
rem Unregister KexShlEx.dll as a COM shell extension
set "DllPath=%~1"

if not defined DllPath (
    set "DllPath=%DEFAULT_TARGET_DIR%\%KEXSHLEXE_NAME%"
)

echo [INFO] Unregistering KexShlEx.dll...

regsvr32 /s /u "%DllPath%"
if %errorlevel% equ 0 (
    echo [INFO] KexShlEx.dll unregistered successfully.
) else (
    echo [ERROR] Failed to unregister KexShlEx.dll (error code: %errorlevel%)
    echo [INFO] You may need to unregister manually:
    echo        regsvr32 /u "%DllPath%"
)

goto :eof

:RegisterShellExtManual
rem Manually register shell extension by adding registry keys
rem This is used when regsvr32 doesn't work (e.g., KexShlEx returns E_NOTIMPL)
echo [INFO] Manually registering KexShlEx shell extension...

set "DllPath=%~1"
if not defined DllPath (
    set "DllPath=%DEFAULT_TARGET_DIR%\%KEXSHLEXE_NAME%"
)

if not exist "%DllPath%" (
    echo [ERROR] %DllPath% does not exist.
    goto :eof
)

rem Get 64-bit reg.exe path for writing to 64-bit registry view
set "RegPath=HKLM\SOFTWARE\Classes\CLSID\%KEXSHLEXE_CLSID%"

echo [INFO] Adding CLSID registration...
reg add "%RegPath%" /v "" /t REG_SZ /d "VxKex Shell Extension" /f >nul
reg add "%RegPath%\InProcServer32" /v "" /t REG_SZ /d "%DllPath%" /f >nul
reg add "%RegPath%\InProcServer32" /v "ThreadingModel" /t REG_SZ /d "Apartment" /f >nul

rem Add property sheet handler extension
set "ExtensionKey=HKLM\SOFTWARE\Classes\Folder\shellex\PropertySheetHandlers\KexShlEx"
echo [INFO] Adding PropertySheetHandler registration...
reg add "%ExtensionKey%" /v "" /t REG_SZ /d "%KEXSHLEXE_CLSID%" /f >nul

echo [INFO] Manual registration completed.
goto :eof

:UnregisterShellExtManual
rem Manually unregister shell extension by removing registry keys
echo [INFO] Manually unregistering KexShlEx shell extension...

rem Remove PropertySheetHandler registration
reg delete "HKLM\SOFTWARE\Classes\Folder\shellex\PropertySheetHandlers\KexShlEx" /f >nul 2>&1

rem Remove CLSID registration
reg delete "HKLM\SOFTWARE\Classes\CLSID\%KEXSHLEXE_CLSID%" /f >nul 2>&1

echo [INFO] Manual unregistration completed.
goto :eof

:ShowUsage
echo.
echo VxKex-Vista Deployment Script
echo Target: Windows Vista SP2 x64
echo.
echo Usage:
echo   %0 [Install|Uninstall] [TargetDir] [ExeName]
echo   %0 install-all [TargetDir]
echo   %0 register [DllPath]
echo   %0 unregister [DllPath]
echo.
echo Examples:
echo   %0 install "C:\VxKex" notepad.exe
echo   %0 uninstall "C:\VxKex" notepad.exe
echo   %0 install-all "C:\VxKex"
echo   %0 register
echo   %0 unregister
echo.
echo Notes:
echo   - This script must be run as Administrator.
echo   - Default target directory: C:\VxKex
echo   - Default target applications: %TARGET_APPS%
echo   - IFEO configuration enables Application Verifier for the target exe.
echo   - Shell extension registration uses manual registry method.
echo.
goto :eof

rem =============================================================================
rem Main
rem =============================================================================

set "Action=%~1"
set "TargetDir=%~2"
set "ExeName=%~3"

if not defined Action (
    call :ShowUsage
    exit /b 0
)

rem Set target directory
if not defined TargetDir (
    set "TargetDir=%DEFAULT_TARGET_DIR%"
)

rem Check administrative privileges for install/uninstall actions
if /i "%Action%" equ "install" (
    call :CheckAdmin
    call :CopyDlls "%TargetDir%"
    
    if not defined ExeName (
        echo [ERROR] Executable name required for install action.
        goto :ShowUsage
    )
    
    call :ApplyIfeo "%TargetDir%" "%ExeName%"
    
    rem Register shell extension
    call :RegisterShellExtManual "%TargetDir%\%KEXSHLEXE_NAME%"
    
    echo.
    echo [INFO] Installation completed for %ExeName%.
    echo [INFO] To test, run %ExeName% and it should be loaded with VxKex.
) else if /i "%Action%" equ "uninstall" (
    call :CheckAdmin
    call :RemoveIfeo "%ExeName%"
    
    rem Unregister shell extension
    call :UnregisterShellExtManual "%TargetDir%\%KEXSHLEXE_NAME%"
    
    echo.
    echo [INFO] Uninstallation completed for %ExeName%.
) else if /i "%Action%" equ "install-all" (
    call :CheckAdmin
    call :CopyDlls "%TargetDir%"
    
    echo [INFO] Installing VxKex for all target applications...
    for %%A in (%TARGET_APPS%) do (
        echo.
        echo --- Configuring %%A ---
        call :ApplyIfeo "%TargetDir%" "%%A"
        call :RegisterShellExtManual "%TargetDir%\%KEXSHLEXE_NAME%"
    )
    
    echo.
    echo [INFO] All-target installation completed.
) else if /i "%Action%" equ "register" (
    call :RegisterShellExtManual "%TargetDir%\%KEXSHLEXE_NAME%"
) else if /i "%Action%" equ "unregister" (
    call :UnregisterShellExtManual "%TargetDir%\%KEXSHLEXE_NAME%"
) else (
    echo [ERROR] Unknown action: %Action%
    echo.
    call :ShowUsage
)

echo.
echo [INFO] Script completed.
pause
