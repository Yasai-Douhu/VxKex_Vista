param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [Parameter(Mandatory=$true)][string]$GuestUser,
    [Parameter(Mandatory=$true)][string]$GuestDirectory,
    [ValidateSet('x86','x64')][string]$Architecture='x64',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity\$Architecture"
$probe="$results\utf8-resource.exe";$guest="$GuestDirectory\$Architecture";$receipt=@()
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\utf8-resource.exe")
foreach($mode in 0..4){
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\utf8-resource.exe" "$guest\KxNt.dll" "$guest\utf8-resource-$mode.txt" "$mode"
    $exit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\utf8-resource-$mode.txt","$results\utf8-resource-$mode.txt")
    $output=[IO.File]::ReadAllText("$results\utf8-resource-$mode.txt")
    if($exit -or $output -notmatch 'Failures=0 Result=MEASURED'){throw "Resource diagnostic failed: $Architecture/$mode"}
    foreach($phase in @('before','immediate','after100ms','after1000ms','warm100ms')){
        if($output -notmatch "Snapshot=$phase Entries=\d+ HandleCount=\d+"){throw 'Handle enumeration incomplete'}
    }
    if($mode -ge 3 -and $output -notmatch 'KexDllLoaded=0'){throw 'Native-only control loaded compatibility DLL'}
    if($mode -lt 3 -and ($output -notmatch 'KexDllLoaded=1' -or $output -notmatch [regex]::Escape("Provider=$guest\KxNt.dll"))){throw 'Unexpected provider in adapter diagnostic'}
    if($mode -lt 3 -and $output -notmatch [regex]::Escape("Implementation=$guest\KexDll.dll")){throw 'Unexpected implementation path'}
    $receipt+=[pscustomobject]@{
        Architecture=$Architecture;Mode=$mode;VMX=$VMX;GuestUser=$GuestUser
        SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_utf8_resource_probe.c").Hash
        ProbeSHA256=(Get-FileHash $probe).Hash;RunnerSHA256=(Get-FileHash $PSCommandPath).Hash
        KexDllSHA256=(Get-FileHash "$results\KexDll.dll").Hash
        KxNtSHA256=(Get-FileHash "$results\KxNt.dll").Hash
        Output=$output;Scope='Separate instrumented probe; empty/lookup/conversion and no-KexDll controls; handle identity/type/name, target process/thread, observation phases; not a zero-leak verdict or original failed-call attribution'
    }
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\utf8-resource-receipt.json"
Write-Host "$Architecture resource observations captured; no original resource gate changed"
