param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity";$receipt=@()
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
foreach($arch in @('x86','x64')) {
    $platform=if($arch -eq 'x86'){'Win32'}else{'x64'};$guest="$GuestDirectory\$arch"
    $files=@{'KexDll.dll'="$root\$platform\Release\KexDll\KexDll.dll";'KxNt.dll'="$root\$platform\Release\KxNt\KxNt.dll";'srw.exe'="$results\$arch\srw.exe"}
    foreach($file in $files.Keys){Guest @('copyFileFromHostToGuest',$VMX,$files[$file],"$guest\$file")}
    & "$results\$arch\srw.exe" native "$results\$arch\srw-host.txt"
    if($LASTEXITCODE){throw "Native host SRW test failed ($arch)"}
    $hostOutput=[IO.File]::ReadAllText("$results\$arch\srw-host.txt")
    Copy-Item $files['KexDll.dll'] "$results\$arch\KexDll.dll" -Force
    Copy-Item $files['KxNt.dll'] "$results\$arch\KxNt.dll" -Force
    & "$results\$arch\srw.exe" "$results\$arch\KxNt.dll" "$results\$arch\srw-host-adapter.txt"
    if($LASTEXITCODE){throw "Native delegation test failed ($arch)"}
    $hostAdapterOutput=[IO.File]::ReadAllText("$results\$arch\srw-host-adapter.txt")
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\srw.exe" native "$guest\srw-native.txt"
    if($LASTEXITCODE -ne 5){throw "Native VM SRW-try absence not confirmed ($arch)"}
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\srw-native.txt","$results\$arch\srw-native-vista.txt")
    $nativeOutput=[IO.File]::ReadAllText("$results\$arch\srw-native-vista.txt")
    if($nativeOutput -notmatch 'Failures=0 Result=NATIVE_TRY_ABSENT'){throw "Native VM SRW layout test failed ($arch)"}
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\srw.exe" "$guest\KxNt.dll" "$guest\srw.txt"
    $probeExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\srw.txt","$results\$arch\srw-vista.txt")
    if($probeExit){throw "SRW VM probe failed ($arch): exit $probeExit; see srw-vista.txt"}
    $output=[IO.File]::ReadAllText("$results\$arch\srw-vista.txt")
    if($output -notmatch 'Failures=0 Result=PASS' -or $hostOutput -notmatch 'Failures=0 Result=PASS'){throw "SRW behavior test failed ($arch)"}
    if($output -notmatch [regex]::Escape("ProviderPath=$guest\KxNt.dll")){throw 'Unexpected provider path'}
    if($output -notmatch [regex]::Escape("KexDllPath=$guest\KexDll.dll")){throw 'Unexpected implementation path'}
    # Ignore paths, DLL loading, and opaque pointer addresses, never Case/Try/Fault results.
    $expected=($hostOutput -split '\r?\n' | Where-Object {$_ -match '^(Case=|Try=|Fault=|Readonly=|ConditionShared=|Layout |Failures=|TryExportsPresent=)'}) -join "`n"
    $actual=($output -split '\r?\n' | Where-Object {$_ -match '^(Case=|Try=|Fault=|Readonly=|ConditionShared=|Layout |Failures=|TryExportsPresent=)'}) -join "`n"
    if($actual -ne $expected){throw "Native behavior differs ($arch)"}
    $delegated=($hostAdapterOutput -split '\r?\n' | Where-Object {$_ -match '^(Case=|Try=|Fault=|Readonly=|ConditionShared=|Layout |Failures=|TryExportsPresent=)'}) -join "`n"
    if($delegated -ne $expected){throw "Native delegation differs ($arch)"}
    $hashes=@{};foreach($file in $files.Keys){$hashes[$file]=(Get-FileHash $files[$file]).Hash}
    $sources=@{};foreach($source in @('KexDll/srwtry.c','KxNt/forwards.c','tests/kxnt_srw_probe.c','tests/run_kxnt_srw_vm.ps1')){$sources[$source]=(Get-FileHash "$root\$source").Hash}
    $receipt += [pscustomobject]@{Architecture=$arch;VMX=$VMX;Hashes=$hashes;SourceSHA256=$sources;HostOutput=$hostOutput;HostAdapterOutput=$hostAdapterOutput;NativeVMOutput=$nativeOutput;Output=$output}
    Write-Host "$arch SRW native interoperation: PASS"
}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$results\srw-receipt.json"
