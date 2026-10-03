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
    $platform=if($arch -eq 'x86'){'Win32'}else{'x64'}
    $guest="$GuestDirectory\$arch"
    $files=@{
        'KexDll.dll'="$root\$platform\Release\KexDll\KexDll.dll"
        'KxNt.dll'="$root\$platform\Release\KxNt\KxNt.dll"
        'KxBase.dll'="$(if($arch -eq 'x86'){"$root\Installer\Kex32"}else{"$root\Installer"})\KxBase.dll"
        'condrv.exe'="$results\$arch\condrv.exe"
        'zig-console-private.exe'="$results\$arch\zig-console-private.exe"
    }
    foreach($file in $files.Keys){Guest @('copyFileFromHostToGuest',$VMX,$files[$file],"$guest\$file")}
    # Get the log even on a failed test; the caller still rejects the failure.
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\condrv.exe" "$guest\condrv.txt" "$guest\KxNt.dll" "$guest\zig-console-private.exe"
    $exit=$LASTEXITCODE
    $log="$results\$arch\condrv-vista.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\condrv.txt",$log)
    $output=[IO.File]::ReadAllText($log)
    if($exit -or $output -notmatch 'Failures=0 Result=PASS'){throw "ConDrv test failed ($arch):`n$output"}
    foreach($module in @('KexDll','KxNt')){if($output -notmatch [regex]::Escape("${module}Path=$guest\$module.dll")){throw "Unexpected loaded module path: $module ($arch)"}}
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\condrv.exe" "$guest\condrv-unprofiled.txt" "$guest\KxNt.dll" "$guest\zig-console-private.exe" 'native-only'
    $exit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\condrv-unprofiled.txt","$results\$arch\condrv-unprofiled.txt")
    $unprofiled=[IO.File]::ReadAllText("$results\$arch\condrv-unprofiled.txt")
    if($exit -or $unprofiled -notmatch 'Failures=0 Result=PASS'){throw "Unprofiled test failed ($arch):`n$unprofiled"}
    $cold=[regex]::Match($output,'Phase=0 NativeControl=0 HandleDelta=(-?\d+)')
    $controlCold=[regex]::Match($unprofiled,'Phase=0 NativeControl=1 HandleDelta=(-?\d+)')
    if(!$cold.Success -or !$controlCold.Success -or $cold.Groups[1].Value -ne $controlCold.Groups[1].Value){throw "Cold handle growth differs from native control ($arch)"}
    foreach($text in @($output,$unprofiled)){
        if($text -notmatch 'Phase=1 NativeControl=[01] HandleDelta=0' -or $text -notmatch 'Case=concurrent-handle-identities Pass=1'){throw "Measured handle snapshot changed ($arch)"}
    }
    $hashes=@{};foreach($file in $files.Keys){$hashes[$file]=(Get-FileHash $files[$file]).Hash}
    $sources=@{};foreach($path in @('KexDll\ntcondrv.c','00-Common-Headers\ZigNtIoProfile.h','tests\kxnt_condrv_probe.c','tests\kxnt_zig_console_smoke.zig')){$sources[$path]=(Get-FileHash "$root\$path").Hash}
    $receipt += [pscustomobject]@{Architecture=$arch;VMX=$VMX;Hashes=$hashes;SourceSHA256=$sources;Output=$output;UnprofiledOutput=$unprofiled;Limitations=@('Raster-font UTF8 rendering retains native OS defects','Partial completion forced only in owned diagnostic KexDll IAT','Ambiguous File/console numeric aliases return NOT_SUPPORTED before I/O','Cold native-control Event creation stack not attributed','No system DLL deployment or full normal IFEO startup integration test')}
    Write-Host "$arch console and real Zig stdio: PASS"
}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$results\condrv-receipt.json"
