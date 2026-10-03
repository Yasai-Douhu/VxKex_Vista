param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity";$receipt=@()
function Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $guest="$GuestDirectory\$arch";$probe="$results\$arch\utf8.exe"
    & $probe native "$results\$arch\utf8-host.txt"
    if($LASTEXITCODE){throw "Native UTF conversion reference failed ($arch)"}
    $hostOutput=[IO.File]::ReadAllText("$results\$arch\utf8-host.txt")
    if($hostOutput -notmatch 'Failures=0 Result=PASS'){throw 'Reference guard/TLS check failed'}
    Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\utf8.exe")
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\utf8.exe" native "$guest\utf8-native.txt"
    $nativeExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\utf8-native.txt","$results\$arch\utf8-native-vista.txt")
    $nativeOutput=[IO.File]::ReadAllText("$results\$arch\utf8-native-vista.txt")
    if($nativeExit -ne 5 -or $nativeOutput -notmatch 'ExportsPresent=0'){throw "Native API absence not confirmed ($arch)"}
    $receipt += [pscustomobject]@{Architecture=$arch;VMX=$VMX;ProbeSHA256=(Get-FileHash $probe).Hash;SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_utf8_probe.c").Hash;Calls=([regex]::Matches($hostOutput,'(?m)^Call=')).Count;HostOutput=$hostOutput;NativeVMExit=$nativeExit;NativeVMOutput=$nativeOutput}
    Write-Host "$arch native UTF conversion reference captured; NT 6.0 exports absent"
}
$fixed=@();foreach($row in $receipt){$fixed += ,(($row.HostOutput -split '\r?\n' | Where-Object {$_ -match '^(Call=|Failures=|ExportsPresent=)'}) -join "`n")}
if($fixed[0] -ne $fixed[1]){throw 'Architecture references differ; inspect before implementing'}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\utf8-reference.json"
