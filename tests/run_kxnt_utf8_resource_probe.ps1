param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [Parameter(Mandatory=$true)][string]$GuestUser,
    [Parameter(Mandatory=$true)][string]$GuestDirectory,
    [ValidateSet('x86','x64')][string]$Architecture='x64',
    [switch]$Windowless,
    [ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity\$Architecture"
$name=if($Windowless){'utf8-resource-windowless'}else{'utf8-resource'}
$probe="$results\$name.exe";$guest="$GuestDirectory\$Architecture";$receipt=@()
$archive=$results
if($Windowless){
    if(!$RunName -or $GuestDirectory -match '\s'){throw 'Windowless mode requires unused RunName and scratch path without whitespace'}
    $archive="$root\audit\KxNtParity\resource-$RunName-$Architecture"
    if(Test-Path $archive){throw 'RunName already exists; preserve earlier observations'}
    New-Item -ItemType Directory $archive | Out-Null
    $pe=[IO.File]::ReadAllBytes($probe);$offset=[BitConverter]::ToInt32($pe,60)
    if([BitConverter]::ToUInt16($pe,$offset+24+68) -ne 2){throw 'Windowless image subsystem invalid'}
}
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\$name.exe")
if($Windowless){
    Guest @('copyFileFromHostToGuest',$VMX,"$results\native-launch.exe","$guest\native-launch.exe")
    $platform=if($Architecture -eq 'x86'){'Win32'}else{'x64'}
    foreach($dll in @('KxNt','KexDll')){Guest @('copyFileFromHostToGuest',$VMX,"$root\$platform\Release\$dll\$dll.dll","$guest\$dll.dll")}
}
$state='Incomplete'
try {
foreach($mode in 0..4){
    $launchOutput=''
    if($Windowless){
        Guest @('runProgramInGuest',$VMX,"$guest\native-launch.exe","$guest\$name.exe","$guest\$name-launch-$mode.txt","$guest\KxNt.dll $guest\$name-$mode.txt $mode")
        Guest @('copyFileFromGuestToHost',$VMX,"$guest\$name-launch-$mode.txt","$archive\launch-$mode.txt")
        $launchOutput=[IO.File]::ReadAllText("$archive\launch-$mode.txt")
        if($launchOutput -notmatch 'CreationFlags=00000000' -or $launchOutput -notmatch 'ChildWait=00000000 ChildExitCode=00000000'){throw 'Windowless child did not naturally exit 0'}
    } else {
        & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\$name.exe" "$guest\KxNt.dll" "$guest\$name-$mode.txt" "$mode"
    }
    $exit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\$name-$mode.txt","$archive\$name-$mode.txt")
    $output=[IO.File]::ReadAllText("$archive\$name-$mode.txt")
    if($exit -or $output -notmatch 'Failures=0 Result=MEASURED'){throw "Resource diagnostic failed: $Architecture/$mode"}
    foreach($phase in @('before','immediate','after100ms','after1000ms','warm100ms')){
        if($output -notmatch "Snapshot=$phase Entries=\d+ HandleCount=\d+"){throw 'Handle enumeration incomplete'}
    }
    if($mode -ge 3 -and $output -notmatch 'KexDllLoaded=0'){throw 'Native-only control loaded compatibility DLL'}
    if($mode -lt 3 -and ($output -notmatch 'KexDllLoaded=1' -or $output -notmatch [regex]::Escape("Provider=$guest\KxNt.dll"))){throw 'Unexpected provider in adapter diagnostic'}
    if($mode -lt 3 -and $output -notmatch [regex]::Escape("Implementation=$guest\KexDll.dll")){throw 'Unexpected implementation path'}
    $receipt+=[pscustomobject]@{
        Architecture=$Architecture;Mode=$mode;VMX=$VMX;GuestUser=$GuestUser
        Windowless=[bool]$Windowless;LaunchOutput=$launchOutput
        SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_utf8_resource_probe.c").Hash
        ProbeSHA256=(Get-FileHash $probe).Hash;RunnerSHA256=(Get-FileHash $PSCommandPath).Hash
        KexDllSHA256=(Get-FileHash $(if($Windowless){"$root\$platform\Release\KexDll\KexDll.dll"}else{"$results\KexDll.dll"})).Hash
        KxNtSHA256=(Get-FileHash $(if($Windowless){"$root\$platform\Release\KxNt\KxNt.dll"}else{"$results\KxNt.dll"})).Hash
        Output=$output;Scope='Separate instrumented probe; empty/lookup/conversion and no-KexDll controls; handle identity/type/name, target process/thread, observation phases; not a zero-leak verdict or original failed-call attribution'
    }
}
$state='Measured'
} finally {
    if($Windowless){[pscustomobject]@{RunName=$RunName;State=$state;Results=$receipt;Scope='Windowless resource observations; no original resource gate changed'}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$archive\receipt.json"}
}
if($Windowless){Write-Host "$Architecture windowless resource observations captured";return}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\utf8-resource-receipt.json"
Write-Host "$Architecture resource observations captured; no original resource gate changed"
