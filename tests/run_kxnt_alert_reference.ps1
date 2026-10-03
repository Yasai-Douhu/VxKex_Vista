param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$results="$root\audit\KxNtParity"
$receipt=@()
function Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $exe="$results\$arch\alert.exe"
    if(!(Test-Path $exe)){throw "Build probes first ($arch)"}
    $hostLog="$results\$arch\alert-native.txt"
    & $exe ntdll.dll $hostLog
    if($LASTEXITCODE){throw "Host native thread-alert reference failed ($arch)"}
    $hostOutput=[IO.File]::ReadAllText($hostLog)
    if($hostOutput -notmatch 'Result=PASS' -or $hostOutput -notmatch 'RaceWaitCalls=1000 InvalidResults=0 Exit=0') {
        throw "Incomplete native reference ($arch)"
    }
    $guest="$GuestDirectory\$arch"
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword directoryExistsInGuest $VMX $guest | Out-Null
    if($LASTEXITCODE){Guest @('createDirectoryInGuest',$VMX,$guest)}
    Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guest\alert-reference.exe")
    # Missing exports are expected on NT 6.0, not a successful emulation test.
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\alert-reference.exe" ntdll.dll "$guest\alert-native.txt"
    $guestExit=$LASTEXITCODE
    $guestLog="$results\$arch\alert-vista-native.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\alert-native.txt",$guestLog)
    $guestOutput=[IO.File]::ReadAllText($guestLog)
    if($guestExit -ne 6 -or $guestOutput -notmatch 'NativeExportsMissing Alert=0 Wait=0') {
        throw "Unexpected NT 6.0 native availability result ($arch): $guestOutput"
    }
    $receipt += [pscustomobject]@{
        Architecture=$arch; ProbeSHA256=(Get-FileHash $exe).Hash;
        HostOSVersion=[Environment]::OSVersion.VersionString;
        HostOutput=$hostOutput; GuestNativeExitCode=$guestExit; GuestNativeOutput=$guestOutput
    }
    Write-Host "$arch native reference: PASS; NT 6.0 exports: absent (expected)"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\alert-reference-receipt.json"
