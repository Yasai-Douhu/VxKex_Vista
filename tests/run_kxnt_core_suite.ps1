param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [Parameter(Mandatory=$true)][string]$GuestUser,
    [Parameter(Mandatory=$true)][string]$GuestDirectory,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
    [ValidateSet(1,3)][int]$ExpectedProductType=1
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$archive=Join-Path "$root\audit\KxNtParity" $RunName
if(Test-Path -LiteralPath $archive){throw 'Use a new RunName; preserve previous evidence'}
New-Item -ItemType Directory -Path $archive | Out-Null
$completed=@();$current=$null;$status='Running';$hashes=@{}
try {
    foreach($arch in @('x86','x64')){
        $release=if($arch -eq 'x86'){'Win32'}else{'x64'}
        $package=if($arch -eq 'x86'){'Installer\Kex32'}else{'Installer'}
        foreach($dll in @('KexDll','KxNt')){
            $hash=(Get-FileHash "$root\$release\Release\$dll\$dll.dll").Hash
            if($hash -ne (Get-FileHash "$root\$package\$dll.dll").Hash){throw "Release/package mismatch: $arch/$dll"}
            $hashes["$arch/$dll"]=$hash
        }
    }
    foreach($probe in @('device-family','processor-feature','domain','persisted-state','sid-package','sid-capability','membership','compare','file-information','alert','performance')){
        $current=$probe
        & "$PSScriptRoot\run_kxnt_processor_feature_vm.ps1" -VMX $VMX -GuestPassword $GuestPassword -GuestUser $GuestUser -GuestDirectory $GuestDirectory -Probe $probe
        $receipt="$root\audit\KxNtParity\$probe-receipt.json"
        Copy-Item -LiteralPath $receipt -Destination "$archive\$probe.json"
        if($probe -eq 'device-family'){
            $rows=Get-Content -Raw $receipt | ConvertFrom-Json
            foreach($row in $rows){
                if($row.Output -notmatch "ReportedVersion=6\.0\.\d+ ProductType=$ExpectedProductType"){throw 'OS family/version does not match requested NT 6.0 environment'}
            }
        }
        $completed+=$probe
    }
    $current=$null;$status='Passed'
} catch {$status='Failed';throw} finally {
    [pscustomobject]@{
        VMX=$VMX;GuestUser=$GuestUser;GuestDirectory=$GuestDirectory
        ExpectedProductType=$ExpectedProductType;Status=$status
        Completed=$completed;CurrentProbe=$current;BinarySHA256=$hashes
        RunnerSHA256=(Get-FileHash $PSCommandPath).Hash
        InnerRunnerSHA256=(Get-FileHash "$PSScriptRoot\run_kxnt_processor_feature_vm.ps1").Hash
        Scope='Explicit scratch DLL loading; per-feature existing semantic gates and native references; no system deployment or IFEO verification'
    } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$archive\manifest.json"
}
Write-Host "Core suite passed: $archive"
