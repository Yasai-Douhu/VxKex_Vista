param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][string]$GuestDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$archive="$root\audit\KxNtParity\open-key-$RunName"
if(Test-Path $archive){throw 'Use a new RunName; retain previous observations'}
New-Item -ItemType Directory $archive|Out-Null
$rows=@();$state='Incomplete'
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
try {
 foreach($arch in @('x86','x64')){
  $probe="$root\audit\KxNtParity\$arch\open-key-options.exe";$guest="$GuestDirectory\$arch";$hostOutputs=@{};$vmOutputs=@{}
  foreach($mode in @('native','legacy')){
   & $probe $mode "$archive\$arch-host-$mode.txt"
   if($LASTEXITCODE){throw 'Host options reference failed'}
   $hostOutputs[$mode]=[IO.File]::ReadAllText("$archive\$arch-host-$mode.txt")
   $expected=if($mode -eq 'native'){81}else{15}
   if([regex]::Matches($hostOutputs[$mode],'(?m)^Method=').Count -ne $expected -or $hostOutputs[$mode] -notmatch 'Failures=0 Result=PASS'){throw 'Host measurement incomplete'}
  }
  Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\open-key-options.exe")
  foreach($mode in @('native','legacy')){
   & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\open-key-options.exe" $mode "$guest\open-key-options-$mode.txt";$code=$LASTEXITCODE
   $expected=if($mode -eq 'native'){5}else{0}
   if($code -ne $expected){throw "Unexpected guest probe exit: $code"}
   Guest @('copyFileFromGuestToHost',$VMX,"$guest\open-key-options-$mode.txt","$archive\$arch-vm-$mode.txt")
   $vmOutputs[$mode]=[IO.File]::ReadAllText("$archive\$arch-vm-$mode.txt")
   if($mode -eq 'native' -and $vmOutputs[$mode] -notmatch 'NativeExPresent=0 Legacy=0[\r\n]+Result=NATIVE_EX_ABSENT'){throw 'Native API absence not confirmed'}
   if($mode -eq 'legacy' -and ([regex]::Matches($vmOutputs[$mode],'(?m)^Method=').Count -ne 15 -or $vmOutputs[$mode] -notmatch 'Failures=0 Result=PASS')){throw 'Legacy measurement incomplete'}
  }
  $rows+=[pscustomobject]@{Architecture=$arch;VMX=$VMX;GuestUser=$GuestUser;ProbeSHA256=(Get-FileHash $probe).Hash;SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_open_key_options_probe.c").Hash;RunnerSHA256=(Get-FileHash $PSCommandPath).Hash;HostNativeOutput=$hostOutputs['native'];HostLegacyOutput=$hostOutputs['legacy'];NativeVMOutput=$vmOutputs['native'];LegacyVMOutput=$vmOutputs['legacy'];Scope='Native read-only OpenOptions/attributes/alignment reference; no adapter implementation or protected-key privilege coverage'}
 }
 $state='Measured'
} finally {
 [pscustomobject]@{State=$state;RunName=$RunName;Results=$rows}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$archive\receipt.json"
}
Write-Host 'Native registry-open options observations recorded'
