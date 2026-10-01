@echo off
setlocal
rem On the current live diagnostic VM, mapped DLLs make the check return 32.
rem The native observer independently verifies that real setup state is unchanged.
call "C:\VxKexProbe\NextParity\RealPackage\install.bat" --check-install > C:\VxKexProbe\NextParity\vistasetup-batch-result.txt 2>&1
set "RESULT=%errorlevel%"
>>C:\VxKexProbe\NextParity\vistasetup-batch-result.txt echo BatchCheckExit=%RESULT%
if not "%RESULT%"=="32" exit /b 1
exit /b 0
