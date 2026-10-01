$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$batch = Get-Content "$root\Installer\install.bat" -Raw
$body = Get-Content "$root\Installer\Remove-VxKex-Files.cmd" -Raw
$body = $body.Replace('"C:\VxKex"', '"C:\VxKexProbe\NextParity\InstallerDeleteFixture\Installed"')
$body = $body.Replace('%windir%\System32', 'C:\VxKexProbe\NextParity\InstallerDeleteFixture\Native')
$body = $body.Replace('%windir%\Sysnative', 'C:\VxKexProbe\NextParity\InstallerDeleteFixture\Native')
$body = $body.Replace('%windir%\SysWOW64', 'C:\VxKexProbe\NextParity\InstallerDeleteFixture\Wow64')
$body = $body.Replace('start explorer.exe', 'rem Explorer restart omitted in this fixture')
$body = [regex]::Replace($body, '(?m)^\s*pause\s*$', '    rem pause omitted in this fixture')
$dllMatch = [regex]::Match($batch, '(?m)^set "X86_DLLS=([^"]+)"')
if (!$dllMatch.Success) { throw 'DLL list not found' }
$dlls = $dllMatch.Groups[1].Value
if ($dlls -notmatch '^[A-Za-z0-9 ]+$') { throw 'Unexpected DLL list' }
$prefix = @'
@echo off
setlocal
set "TARGET_DIR=C:\VxKexProbe\NextParity\InstallerDeleteFixture\Installed"
set "X86_DLLS=__DLLS__"
set "VXKEX_UNINSTALL_PREPARED=1"
set "REMOVAL_HELPER=C:\VxKexProbe\NextParity\installer-delete-body.cmd"
cd /d "%TARGET_DIR%"
"%REMOVAL_HELPER%"
exit /b 1
'@
$shim = $prefix.Replace('__DLLS__', $dlls)
if ($body -match 'del[^\r\n]*%windir%' -or $body -match 'Kx\*\.dll') { throw 'Real system path or wildcard in deletion fixture' }
$test = @'
@echo off
setlocal
set "FIXTURE=C:\VxKexProbe\NextParity\InstallerDeleteFixture"
set "FAILURES=0"
if exist "%FIXTURE%" (
    echo Existing fixture refused.
    exit /b 3
)
mkdir "%FIXTURE%\Native"
mkdir "%FIXTURE%\Wow64"
mkdir "%FIXTURE%\Installed"
echo VxKex-next-parity-fixture>"%FIXTURE%\FixtureOwner.txt"
copy /y C:\VxKexProbe\NextParity\installer-delete-shim.cmd "%FIXTURE%\Installed\install.bat" >nul
for %%D in (__DLLS__ KxSChanl) do echo NativeTestDLL>"%FIXTURE%\Native\%%D.dll"
for %%D in (__DLLS__) do echo Wow64TestDLL>"%FIXTURE%\Wow64\%%D.dll"
echo ForeignNativeDLL>"%FIXTURE%\Native\KxUnrelated.dll"
echo ForeignWow64DLL>"%FIXTURE%\Wow64\KxUnrelated.dll"
call "%FIXTURE%\Installed\install.bat"
if errorlevel 1 set /a FAILURES+=1
if exist "%FIXTURE%\Installed" set /a FAILURES+=1
for %%D in (__DLLS__ KxSChanl) do if exist "%FIXTURE%\Native\%%D.dll" set /a FAILURES+=1
for %%D in (__DLLS__) do if exist "%FIXTURE%\Wow64\%%D.dll" set /a FAILURES+=1
if not exist "%FIXTURE%\Native\KxUnrelated.dll" set /a FAILURES+=1
if not exist "%FIXTURE%\Wow64\KxUnrelated.dll" set /a FAILURES+=1
if "%FAILURES%"=="0" rmdir /s /q "%FIXTURE%"
echo Failures=%FAILURES%
exit /b %FAILURES%
'@
$test = $test.Replace('__DLLS__', $dlls)
[IO.File]::WriteAllText("$root\audit\installer-delete-body.cmd", ($body -replace '\r?\n', "`r`n"), [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$root\audit\installer-delete-shim.cmd", ($shim -replace '\r?\n', "`r`n"), [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$root\audit\installer-delete-fixture.cmd", ($test -replace '\r?\n', "`r`n"), [Text.Encoding]::ASCII)
