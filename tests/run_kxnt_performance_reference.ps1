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
    $exe="$results\$arch\performance.exe"
    $hostLog="$results\$arch\performance-native.txt"
    & $exe ntdll.dll $hostLog
    if($LASTEXITCODE){throw "Host native performance reference failed ($arch)"}
    $hostOutput=[IO.File]::ReadAllText($hostLog)
    $guest="$GuestDirectory\$arch"
    Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guest\performance-reference.exe")
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\performance-reference.exe" ntdll.dll "$guest\performance-nt-native.txt"
    $nativeExit=$LASTEXITCODE
    $nativeLog="$results\$arch\performance-nt-native.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\performance-nt-native.txt",$nativeLog)
    $nativeOutput=[IO.File]::ReadAllText($nativeLog)
    if($nativeExit -ne 5 -or $nativeOutput -notmatch 'MissingExports Counter=0 Frequency=0'){throw "Unexpected NT 6.0 RTL availability ($arch)"}
    # NT 6.0 Win32 is a contrasting oracle, not the modern RTL contract.
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\performance-reference.exe" kernel32.dll "$guest\performance-win32.txt" Win32
    $win32Exit=$LASTEXITCODE
    $win32Log="$results\$arch\performance-win32.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\performance-win32.txt",$win32Log)
    $win32Output=[IO.File]::ReadAllText($win32Log)
    if($win32Exit -ne 1 -or $win32Output -notmatch 'Bad=counter-null Exception=00000000 LastError=000003e6' -or $win32Output -notmatch 'Result=FAIL'){throw "Unexpected NT 6.0 Win32 pointer behavior ($arch)"}
    $receipt += [pscustomobject]@{
        Architecture=$arch; ProbeSHA256=(Get-FileHash $exe).Hash;
        HostOSVersion=[Environment]::OSVersion.VersionString;HostOutput=$hostOutput;
        GuestNativeExitCode=$nativeExit;GuestNativeOutput=$nativeOutput;
        GuestWin32ExitCode=$win32Exit;GuestWin32Output=$win32Output
    }
    Write-Host "$arch performance reference: modern RTL PASS; NT 6.0 Win32 differences verified"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\performance-reference-receipt.json"
