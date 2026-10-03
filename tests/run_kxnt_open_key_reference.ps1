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
    if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $probe="$results\$arch\open-key-reference.exe";$guest="$GuestDirectory\$arch"
    foreach($mode in @('native','legacy')) {
        & $probe $mode "$results\$arch\open-key-$mode-host.txt"
        if($LASTEXITCODE){throw "Host key reference failed ($arch, $mode)"}
    }
    Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\open-key-reference.exe")
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\open-key-reference.exe" native "$guest\open-key-native.txt"
    $nativeExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\open-key-native.txt","$results\$arch\open-key-native-vista.txt")
    $absent=[IO.File]::ReadAllText("$results\$arch\open-key-native-vista.txt")
    if($nativeExit -ne 5 -or $absent -notmatch 'Result=NATIVE_EX_ABSENT'){throw "Native absence not confirmed ($arch)"}
    Guest @('runProgramInGuest',$VMX,"$guest\open-key-reference.exe",'legacy',"$guest\open-key-legacy.txt")
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\open-key-legacy.txt","$results\$arch\open-key-legacy-vista.txt")
    $legacy=[IO.File]::ReadAllText("$results\$arch\open-key-legacy-vista.txt")
    $hostLegacy=[IO.File]::ReadAllText("$results\$arch\open-key-legacy-host.txt")
    if($legacy -notmatch 'Failures=0 Result=PASS'){throw 'Legacy opening failed'}
    # Ignore control-set number and key-name casing; assert actual source/target identity.
    if($legacy -notmatch 'Input=system-link Options=00000000 Attributes=00000140 Status=00000000.*Name=\\Registry\\Machine\\SYSTEM\\CurrentControlSet' -or $legacy -notmatch 'Input=system-link Options=00000000 Attributes=00000040 Status=00000000.*Name=\\Registry\\Machine\\SYSTEM\\ControlSet\d+'){throw 'Link source/target distinction failed'}
    $receipt += [pscustomobject]@{Architecture=$arch;VMX=$VMX;ProbeSHA256=(Get-FileHash $probe).Hash;SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_open_key_reference_probe.c").Hash;HostNativeOutput=[IO.File]::ReadAllText("$results\$arch\open-key-native-host.txt");HostLegacyOutput=$hostLegacy;NativeVMOutput=$absent;LegacyVMOutput=$legacy;Scope='Read-only normal/system symbolic-link/missing key reference. No compatibility implementation or backup privilege enablement.'}
    Write-Host "$arch registry link opening with OBJ_OPENLINK verified; native Ex absent"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\open-key-reference.json"
