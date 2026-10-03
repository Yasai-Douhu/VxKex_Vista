param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [switch]$TraceResources,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$archive="$root\audit\KxNtParity\windowless-$RunName"
if(Test-Path $archive){throw 'RunName already exists; preserve previous measurements'}
if($GuestDirectory -match '\s'){throw 'This diagnostic requires a scratch guest path without whitespace'}
New-Item -ItemType Directory $archive | Out-Null
$rows=@();$state='Incomplete';$current=''
$probeName=if($TraceResources){'utf8-traced-windowless.exe'}else{'utf8-windowless.exe'}
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}}
function Fixed([string]$Text){return (($Text -split '\r?\n' | Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Parallel=|Overlap=|Failures=|ExportsPresent=)'}) -join "`n") -replace '(Parallel=4 Phase=0 Calls=16000 Errors=0 HandleDelta=)\d+','$1<environment>'}
try {
 foreach($arch in @('x86','x64')) {
  $current=$arch;$bin="$root\audit\KxNtParity\$arch";$guest="$GuestDirectory\$arch"
  $platform=if($arch -eq 'x86'){'Win32'}else{'x64'}
  $files=@{$probeName="$bin\$probeName";'native-launch.exe'="$bin\native-launch.exe";'KxNt.dll'="$root\$platform\Release\KxNt\KxNt.dll";'KexDll.dll'="$root\$platform\Release\KexDll\KexDll.dll"}
  $pe=[IO.File]::ReadAllBytes($files[$probeName]);$offset=[BitConverter]::ToInt32($pe,60)
  if([BitConverter]::ToUInt16($pe,$offset+24+68) -ne 2){throw 'Probe is not Windows subsystem'}
  $hashes=@{};foreach($file in $files.Keys){$hashes[$file]=(Get-FileHash $files[$file]).Hash;Guest @('copyFileFromHostToGuest',$VMX,$files[$file],"$guest\$file")}
  & "$bin\utf8.exe" native "$archive\$arch-native.txt"
  if($LASTEXITCODE){throw 'Console native reference failed'}
  $native=[IO.File]::ReadAllText("$archive\$arch-native.txt")
  $outputs=@{};$launches=@{}
  foreach($mode in @('lookup','conversion')) {
   $tail="$guest\KxNt.dll $guest\utf8-windowless-$mode.txt"
   if($mode -eq 'lookup'){$tail+=' lookup-control'}
   Guest @('runProgramInGuest',$VMX,"$guest\native-launch.exe","$guest\$probeName","$guest\utf8-windowless-launch-$mode.txt",$tail)
   foreach($prefix in @('utf8-windowless-','utf8-windowless-launch-')){Guest @('copyFileFromGuestToHost',$VMX,"$guest\$prefix$mode.txt","$archive\$arch-$prefix$mode.txt")}
   $launches[$mode]=[IO.File]::ReadAllText("$archive\$arch-utf8-windowless-launch-$mode.txt")
   if($launches[$mode] -notmatch 'CreationFlags=00000000' -or $launches[$mode] -notmatch 'ChildWait=00000000 ChildExitCode=00000000'){throw 'Windowless child did not naturally exit successfully'}
   $outputs[$mode]=[IO.File]::ReadAllText("$archive\$arch-utf8-windowless-$mode.txt")
   if($outputs[$mode] -notmatch [regex]::Escape("Provider=$guest\KxNt.dll") -or $outputs[$mode] -notmatch [regex]::Escape("Implementation=$guest\KexDll.dll")){throw 'Unexpected runtime DLL paths'}
  }
  $contract=(Fixed $outputs['conversion']) -ceq (Fixed $native)
  $cold=[regex]::Match($outputs['conversion'],'Phase=0 Calls=16000 Errors=0 HandleDelta=(\d+)')
  $controlCold=[regex]::Match($outputs['lookup'],'Phase=0 Calls=8000 Errors=0 HandleDelta=(\d+)')
  $resource=$cold.Success -and $controlCold.Success -and $cold.Groups[1].Value -eq $controlCold.Groups[1].Value -and $outputs['lookup'] -match 'Phase=1 Calls=8000 Errors=0 HandleDelta=0' -and $outputs['lookup'] -match 'Failures=0 Result=CONTROL'
  $rows+=[pscustomobject]@{Architecture=$arch;VMX=$VMX;GuestUser=$GuestUser;GuestDirectory=$guest;Subsystem=2;CreationFlags=0;Hashes=$hashes;NativeProbeSHA256=(Get-FileHash "$bin\utf8.exe").Hash;NativeOutput=$native;Output=$outputs['conversion'];ControlOutput=$outputs['lookup'];Launches=$launches;ContractGate=$contract;ColdResourceGate=$resource;ColdDelta=$cold.Groups[1].Value;ControlColdDelta=$controlCold.Groups[1].Value}
  Write-Host "$arch windowless UTF: Contract=$contract ColdResource=$resource"
 }
 $state=if(@($rows|Where-Object {!$_.ContractGate -or !$_.ColdResourceGate}).Count){'Failed'}else{'Passed'}
} finally {
 $sources=@{};foreach($file in @('KexDll/utf8.c','tests/kxnt_utf8_probe.c','tests/kxnt_utf8_trace.h','tests/kxnt_native_launch_probe.c','tests/build_kxnt_probes.ps1','tests/run_kxnt_utf8_windowless_vm.ps1')){$sources[$file]=(Get-FileHash "$root\$file").Hash}
 [pscustomobject]@{RunName=$RunName;Status=$state;TraceResources=[bool]$TraceResources;CurrentArchitecture=$current;SourceSHA256=$sources;Results=$rows;Scope='Additional Windows-subsystem diagnostic using identical UTF source and cases; optional resource snapshots perturb timing; original console cold-resource failure remains separate; no IFEO or system deployment proof'} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 "$archive\receipt.json"
}
if($state -ne 'Passed'){throw "Windowless comparison gate failed; evidence preserved in $archive"}
