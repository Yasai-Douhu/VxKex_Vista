@echo off
setlocal
set "HELPER=C:\VxKexProbe\NextParity\VistaSetup.exe"
set "LOG=C:\VxKexProbe\NextParity\vistasetup-cli-result.txt"
"%HELPER%" --current-user-sid > C:\VxKexProbe\NextParity\vistasetup-current-sid.txt 2>&1
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%S in ("C:\VxKexProbe\NextParity\vistasetup-current-sid.txt") do set "ORIGINAL_SID=%%S"
if not defined ORIGINAL_SID exit /b 1
"%HELPER%" --unknown > "%LOG%" 2>&1
if not errorlevel 87 exit /b 1
if errorlevel 88 exit /b 1
>>"%LOG%" echo InvalidArgumentsExit=87
"%HELPER%" --check-uninstall-remove >> "%LOG%" 2>&1
if not errorlevel 87 exit /b 1
if errorlevel 88 exit /b 1
>>"%LOG%" echo MissingInitiatorSidExit=87
"%HELPER%" --check-uninstall-remove --user-sid "S-1-5-18" >> "%LOG%" 2>&1
if not errorlevel 87 exit /b 1
if errorlevel 88 exit /b 1
>>"%LOG%" echo InvalidInitiatorSidExit=87
"%HELPER%" --check-install "C:\VxKexProbe\NextParity\MissingPackage" >> "%LOG%" 2>&1
if not errorlevel 2 exit /b 1
if errorlevel 3 exit /b 1
>>"%LOG%" echo MissingPackageExit=2
rem The following checks may encounter mapped DLLs in the running installation.
rem Observe their exact result; all check-only modes must roll back either way.
"%HELPER%" --check-install "C:\VxKexProbe\NextParity\RealPackage" --user-sid "%ORIGINAL_SID%" >> "%LOG%" 2>&1
>>"%LOG%" echo CheckInstallExit=%errorlevel%
"%HELPER%" --check-uninstall-keep --user-sid "%ORIGINAL_SID%" >> "%LOG%" 2>&1
>>"%LOG%" echo CheckKeepExit=%errorlevel%
"%HELPER%" --check-uninstall-remove --user-sid "%ORIGINAL_SID%" >> "%LOG%" 2>&1
>>"%LOG%" echo CheckRemoveExit=%errorlevel%
exit /b 0
