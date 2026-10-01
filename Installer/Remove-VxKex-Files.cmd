@echo off
setlocal
:: This helper runs from a protected cache outside the directory being removed.
if not "%VXKEX_UNINSTALL_PREPARED%"=="1" exit /b 87
if /i not "%TARGET_DIR%"=="C:\VxKex" exit /b 87
if not defined X86_DLLS exit /b 87
if exist "%windir%\Sysnative\cmd.exe" (
    set "SYS32_DIR=%windir%\Sysnative"
) else (
    set "SYS32_DIR=%windir%\System32"
)
cd /d "%windir%"
if errorlevel 1 exit /b 1
:: This file remains outside TARGET_DIR. Exit truthfully on locked files.
(
    for %%D in (%X86_DLLS% KxSChanl) do (
        del /f /q "%SYS32_DIR%\%%D.dll" >nul 2>&1
        if exist "%SYS32_DIR%\%%D.dll" (
            echo Could not remove %%D.dll from System32. Close applications and retry from the complete package.
            start explorer.exe
            pause
            exit /b 1
        )
    )
    for %%D in (%X86_DLLS%) do (
        del /f /q "%windir%\SysWOW64\%%D.dll" >nul 2>&1
        if exist "%windir%\SysWOW64\%%D.dll" (
            echo Could not remove %%D.dll from SysWOW64. Close applications and retry from the complete package.
            start explorer.exe
            pause
            exit /b 1
        )
    )
    echo [*] Removing %TARGET_DIR%...
    if exist "%TARGET_DIR%" rmdir /s /q "%TARGET_DIR%" >nul 2>&1
    if exist "%TARGET_DIR%" (
        echo The installation directory could not be removed. Retry from the complete package.
        start explorer.exe
        pause
        exit /b 1
    )
    echo [*] Restarting explorer.exe...
    start explorer.exe
    echo.
    echo ========================================================
    echo   VxKex ^(x64^) has been successfully uninstalled.
    echo ========================================================
    echo.
    pause
    exit /b 0
)

