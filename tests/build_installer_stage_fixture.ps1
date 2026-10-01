$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$batch = Get-Content "$root\Installer\install.bat" -Raw
$match = [regex]::Match($batch, '(?m)^:stage-removal-helper\r?\n[\s\S]*$')
if (!$match.Success) { throw 'Staging routine missing' }
$routine = $match.Value.Replace('%ProgramData%\VxKexVistaSetup', 'C:\VxKexProbe\NextParity\InstallerStageFixture')
$test = @'
@echo off
setlocal
set "FAILURES=0"
set "SCRIPT_DIR=C:\VxKexProbe\NextParity\"
if exist C:\VxKexProbe\NextParity\InstallerStageFixture (
    echo Existing fixture refused.
    exit /b 3
)
call :stage-removal-helper
if errorlevel 1 exit /b 1
"%windir%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File C:\VxKexProbe\NextParity\installer-stage-acl.ps1
if errorlevel 1 set /a FAILURES+=1
:: Simulate stale explicit child and directory permissions in a previous cache.
icacls "%REMOVAL_CACHE%" /grant *S-1-5-32-545:^(OI^)^(CI^)F >nul
if errorlevel 1 set /a FAILURES+=1
icacls "%REMOVAL_HELPER%" /grant *S-1-5-32-545:F >nul
if errorlevel 1 set /a FAILURES+=1
call :stage-removal-helper
if errorlevel 1 set /a FAILURES+=1
"%windir%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File C:\VxKexProbe\NextParity\installer-stage-acl.ps1
if errorlevel 1 set /a FAILURES+=1
if "%FAILURES%"=="0" rmdir /s /q C:\VxKexProbe\NextParity\InstallerStageFixture
echo Failures=%FAILURES%
exit /b %FAILURES%

__STAGING_ROUTINE__
'@
$acl = @'
$ErrorActionPreference = 'Stop'
$cache = 'C:\VxKexProbe\NextParity\InstallerStageFixture'
$paths = @($cache, "$cache\Owner.txt", "$cache\Remove-VxKex-Files.cmd")
foreach ($path in $paths) {
    $security = Get-Acl -LiteralPath $path
    $owner = $security.GetOwner([System.Security.Principal.SecurityIdentifier]).Value
    if ($owner -ne 'S-1-5-32-544') { throw "Unexpected owner of $path" }
    $rules = @($security.GetAccessRules($true, $true, [System.Security.Principal.SecurityIdentifier]))
    if ($rules.Count -ne 2) { throw "Unexpected ACE count for $path" }
    $identities = @()
    foreach ($rule in $rules) {
        if ($rule.AccessControlType -ne 'Allow' -or ($rule.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::FullControl) -ne [System.Security.AccessControl.FileSystemRights]::FullControl) {
            throw "Unexpected access rule for $path"
        }
        $identities += $rule.IdentityReference.Value
    }
    if ($identities -notcontains 'S-1-5-18' -or $identities -notcontains 'S-1-5-32-544') { throw "Unexpected principals for $path" }
    if ($path -eq $cache -and !$security.AreAccessRulesProtected) { throw 'Cache inherits an untrusted DACL' }
    Write-Output "PASS restricted DACL and Administrators owner: $path"
}
'@
$test = $test.Replace('__STAGING_ROUTINE__', $routine)
[IO.File]::WriteAllText("$root\audit\installer-stage-fixture.cmd", ($test -replace '\r?\n', "`r`n"), [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$root\audit\installer-stage-acl.ps1", $acl, [Text.Encoding]::ASCII)
