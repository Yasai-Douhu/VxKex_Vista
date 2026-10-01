$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$batch = Get-Content "$root\Installer\install.bat" -Raw
# The first occurrence can be a call; require a real label at the line start.
$match = [regex]::Match($batch, '(?m)^:ensure-logdir\r?\n[\s\S]*$')
if (!$match.Success) { throw 'LogDir routine missing' }
$logdir = $match.Value
$start = $batch.IndexOf(':: 2. Remove Registry Configuration')
$end = $batch.IndexOf(':: 3. Stop explorer.exe to unlock files', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Registry uninstall block missing' }
$remove = $batch.Substring($start, $end - $start)
$fixture = 'HKCU\Software\VxKexInstallerFixture-20261001'
foreach ($name in @('logdir', 'remove')) {
    $part = Get-Variable $name -ValueOnly
    $part = $part.Replace('HKLM\SOFTWARE\VXsoft', "$fixture\Machine\VXsoft")
    $part = $part.Replace('HKCU\Software\VXsoft', "$fixture\User\VXsoft")
    if ($part -match 'HKLM|HKCR' -or $part -match '(?i)HKCU\\Software\\VXsoft') { throw 'Unmapped real registry path' }
    Set-Variable $name $part
}
$test = @'
@echo off
setlocal
set "REG_CMD=%windir%\System32\reg.exe"
set "TARGET_DIR=C:\Fixture"
set "FIXTURE=HKCU\Software\VxKexInstallerFixture-20261001"
set "FAILURES=0"
"%REG_CMD%" query "%FIXTURE%" >nul 2>&1
if not errorlevel 1 (
    echo Existing fixture refused.
    exit /b 3
)
"%REG_CMD%" add "%FIXTURE%" /v FixtureOwner /t REG_SZ /d VxKex-next-parity /f >nul
call :ensure-logdir
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v LogDir | findstr /L /C:"C:\Fixture\Logs" >nul
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\VxKex" /v LogDir /t REG_SZ /d "C:\Custom Logs" /f >nul
call :ensure-logdir
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v LogDir | findstr /L /C:"C:\Custom Logs" >nul
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\VxKex" /v InstalledVersion /t REG_DWORD /d 1 /f >nul
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\VxKex" /v KexDir /t REG_SZ /d "C:\Fixture" /f >nul
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\VxKex" /v LoggingEnabled /t REG_DWORD /d 1 /f >nul
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\OtherProduct" /v ForeignValue /t REG_DWORD /d 123 /f >nul
"%REG_CMD%" add "%FIXTURE%\Machine\VXsoft\VxKexVistaPreserved" /v StoreVersion /t REG_DWORD /d 1 /f >nul
"%REG_CMD%" add "%FIXTURE%\User\VXsoft\VxKex" /v UserPreference /t REG_DWORD /d 456 /f >nul
"%REG_CMD%" add "%FIXTURE%\User\VXsoft\OtherProduct" /v ForeignValue /t REG_DWORD /d 789 /f >nul
set "KEEP_SETTINGS=1"
call :remove-registry
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v InstalledVersion >nul 2>&1
if not errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v KexDir >nul 2>&1
if not errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v LogDir | findstr /L /C:"C:\Custom Logs" >nul
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" /v LoggingEnabled >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\User\VXsoft\VxKex" /v UserPreference >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKexVistaPreserved" /v StoreVersion >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
set "KEEP_SETTINGS=0"
call :remove-registry
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKex" >nul 2>&1
if not errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\User\VXsoft\VxKex" >nul 2>&1
if not errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\OtherProduct" /v ForeignValue >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\User\VXsoft\OtherProduct" /v ForeignValue >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" query "%FIXTURE%\Machine\VXsoft\VxKexVistaPreserved" /v StoreVersion >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
"%REG_CMD%" delete "%FIXTURE%" /f >nul 2>&1
if errorlevel 1 set /a FAILURES+=1
echo Failures=%FAILURES%
exit /b %FAILURES%

__LOGDIR_ROUTINE__

:remove-registry
__REMOVE_REGISTRY_BLOCK__
exit /b 0
'@
$test = $test.Replace('__LOGDIR_ROUTINE__', $logdir).Replace('__REMOVE_REGISTRY_BLOCK__', $remove)
$test = $test -replace '\r?\n', "`r`n"
[IO.File]::WriteAllText("$root\audit\installer-registry-fixture.cmd", $test, [Text.Encoding]::ASCII)
