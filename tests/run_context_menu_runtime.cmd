@echo off
setlocal DisableDelayedExpansion
fc /b C:\VxKexProbe\NextParity\DisposableVM.txt C:\VxKexProbe\NextParity\ExpectedDisposableVM.txt >nul
if errorlevel 1 exit /b 5
if exist C:\VxKex exit /b 183
reg query HKCU\Software\VXsoft\VxKex >nul 2>&1
if not errorlevel 1 exit /b 183
set FAILURES=0
set "SID="
for /f "delims=" %%S in ('C:\VxKexProbe\NextParity\VistaSetup.exe --current-user-sid') do set "SID=%%S"
if not defined SID exit /b 5
C:\VxKexProbe\NextParity\VistaSetup.exe --install C:\VxKexProbe\NextParity\RealPackage --user-sid %SID% > C:\VxKexProbe\NextParity\context-runtime-install.txt 2>&1
if errorlevel 1 exit /b %errorlevel%
C:\VxKex\VistaRun.exe --with-kex "C:\VxKexProbe\NextParity\Native\VxKexParityRuntime20261001.exe" --report C:\VxKexProbe\NextParity\context-runtime-native.txt --unspoofed > C:\VxKexProbe\NextParity\context-runtime-launch.txt 2>&1
set "EXE_RESULT=%errorlevel%"
if not "%EXE_RESULT%"=="0" set /a FAILURES+=1
echo ExeMenuLaunchCode=%EXE_RESULT% > C:\VxKexProbe\NextParity\context-runtime-status.txt
C:\VxKex\VistaRun.exe --with-kex "C:\VxKexProbe\NextParity\Wow\VxKexParityRuntime20261001.exe" --report C:\VxKexProbe\NextParity\context-runtime-wow.txt --unspoofed >> C:\VxKexProbe\NextParity\context-runtime-launch.txt 2>&1
set "WOW_RESULT=%errorlevel%"
if not "%WOW_RESULT%"=="0" set /a FAILURES+=1
echo WowExeMenuLaunchCode=%WOW_RESULT% >> C:\VxKexProbe\NextParity\context-runtime-status.txt
C:\VxKex\VistaRun.exe --with-kex C:\Windows\System32\msiexec.exe /i C:\VxKexProbe\NextParity\NoSuchMenuProbe20261001.msi /qn >> C:\VxKexProbe\NextParity\context-runtime-launch.txt 2>&1
set "MSI_RESULT=%errorlevel%"
if not "%MSI_RESULT%"=="1619" set /a FAILURES+=1
echo MsiMenuLaunchCode=%MSI_RESULT% >> C:\VxKexProbe\NextParity\context-runtime-status.txt
C:\VxKexProbe\NextParity\VistaSetup.exe --uninstall-remove --user-sid %SID% > C:\VxKexProbe\NextParity\context-runtime-remove.txt 2>&1
if errorlevel 1 set /a FAILURES+=1
echo Failures=%FAILURES% >> C:\VxKexProbe\NextParity\context-runtime-status.txt
exit /b %FAILURES%
