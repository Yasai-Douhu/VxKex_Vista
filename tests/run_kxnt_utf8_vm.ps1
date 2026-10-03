param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity";$receipt=@()
& "$PSScriptRoot\run_kxnt_utf8_reference.ps1" @PSBoundParameters
function Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
function Fixed([string]$Output) {return (($Output -split '\r?\n' | Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Parallel=|Overlap=|Failures=|ExportsPresent=)'}) -join "`n") -replace '(Parallel=4 Phase=0 Calls=16000 Errors=0 HandleDelta=)\d+','$1<environment>'}
foreach($arch in @('x86','x64')) {
    $platform=if($arch -eq 'x86'){'Win32'}else{'x64'};$guest="$GuestDirectory\$arch"
    $files=@{'KexDll.dll'="$root\$platform\Release\KexDll\KexDll.dll";'KxNt.dll'="$root\$platform\Release\KxNt\KxNt.dll";'utf8.exe'="$results\$arch\utf8.exe"}
    foreach($file in $files.Keys){Guest @('copyFileFromHostToGuest',$VMX,$files[$file],"$guest\$file");if($file -ne 'utf8.exe'){Copy-Item $files[$file] "$results\$arch\$file" -Force}}
    & "$results\$arch\utf8.exe" "$results\$arch\KxNt.dll" "$results\$arch\utf8-host-adapter.txt"
    if($LASTEXITCODE){throw "Native delegation failed ($arch)"}
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\utf8.exe" "$guest\KxNt.dll" "$guest\utf8-control.txt" 'lookup-control'
    $controlExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\utf8-control.txt","$results\$arch\utf8-control-vista.txt")
    $control=[IO.File]::ReadAllText("$results\$arch\utf8-control-vista.txt")
    if($controlExit){throw "Native lookup control failed ($arch): exit $controlExit; see utf8-control-vista.txt"}
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\utf8.exe" "$guest\KxNt.dll" "$guest\utf8.txt"
    $probeExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\utf8.txt","$results\$arch\utf8-vista.txt")
    if($probeExit){throw "UTF conversion failed ($arch): exit $probeExit; see utf8-vista.txt"}
    $output=[IO.File]::ReadAllText("$results\$arch\utf8-vista.txt")
    $hostOutput=[IO.File]::ReadAllText("$results\$arch\utf8-host.txt")
    $delegated=[IO.File]::ReadAllText("$results\$arch\utf8-host-adapter.txt")
    $cold=[regex]::Match($output,'Parallel=4 Phase=0 Calls=16000 Errors=0 HandleDelta=(\d+)')
    $coldControl=[regex]::Match($control,'Parallel=4 Phase=0 Calls=8000 Errors=0 HandleDelta=(\d+)')
    if(!$cold.Success -or !$coldControl.Success -or $cold.Groups[1].Value -ne $coldControl.Groups[1].Value -or $control -notmatch 'Phase=1 Calls=8000 Errors=0 HandleDelta=0' -or $control -notmatch 'Failures=0 Result=CONTROL'){throw "Cold resource difference unresolved ($arch)"}
    if((Fixed $output) -ne (Fixed $hostOutput) -or (Fixed $delegated) -ne (Fixed $hostOutput)) {
        Compare-Object ((Fixed $hostOutput) -split "`n") ((Fixed $output) -split "`n") | Select-Object -First 4 | Format-List | Out-Host
        throw "UTF contract differs ($arch)"
    }
    if($output -notmatch [regex]::Escape("Provider=$guest\KxNt.dll")){throw 'Unexpected provider path'}
    if($output -notmatch [regex]::Escape("Implementation=$guest\KexDll.dll")){throw 'Unexpected implementation path'}
    $hashes=@{};foreach($file in $files.Keys){$hashes[$file]=(Get-FileHash $files[$file]).Hash}
    $sources=@{};foreach($source in @('KexDll/utf8.c','KxNt/forwards.c','tests/kxnt_utf8_probe.c','tests/run_kxnt_utf8_vm.ps1')){$sources[$source]=(Get-FileHash "$root\$source").Hash}
    $receipt += [pscustomobject]@{Architecture=$arch;VMX=$VMX;Hashes=$hashes;SourceSHA256=$sources;HostAdapterOutput=$delegated;LookupControlOutput=$control;Output=$output;Limitations=@('Cold native lookup resource creation stack not attributed','No Vista client or normal IFEO integration verification','4GB size arithmetic overflow explicitly refused; not covered by native comparison')}
    Write-Host "$arch UTF conversion native comparison: PASS"
}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$results\utf8-receipt.json"
