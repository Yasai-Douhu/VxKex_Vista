param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][string]$GuestDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$archive="$root\audit\KxNtParity\open-key-adapter-$RunName"
if(Test-Path $archive){throw 'Archive exists; retain previous evidence'}
New-Item -ItemType Directory $archive|Out-Null
$rows=@();$state='Incomplete'
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
function Check([string]$Text,[string]$Directory,[int]$Native){
 if($Text -notmatch [regex]::Escape("Provider=$Directory\KxNt.dll") -or $Text -notmatch [regex]::Escape("Implementation=$Directory\KexDll.dll NativePresent=$Native AliasEqual=1")){throw 'Wrong runtime DLL path or native route'}
 if([regex]::Matches($Text,'(?m)^Case=').Count -ne 280 -or $Text -match 'Match=0' -or $Text -notmatch 'Repeated=1000 HandleDelta=0' -or $Text -notmatch 'Failures=0 Result=PASS'){throw 'Native pair/resource comparison failed'}
 if(!$Native -and [regex]::Matches($Text,'(?m)^Unsupported=.*Status=c00000bb OutputUntouched=1').Count -ne 7){throw 'Unimplemented extended option rejection failed'}
 if($Native -and [regex]::Matches($Text,'(?m)^Delegated=.*Match=1').Count -ne 8){throw 'Native extended-option delegation failed'}
}
try {
 foreach($arch in @('x86','x64')){
  $platform=if($arch -eq 'x86'){'Win32'}else{'x64'};$bin="$root\audit\KxNtParity\$arch";$guest="$GuestDirectory\$arch"
  $files=@{'open-key-adapter.exe'="$bin\open-key-adapter.exe";'KxNt.dll'="$root\$platform\Release\KxNt\KxNt.dll";'KexDll.dll'="$root\$platform\Release\KexDll\KexDll.dll"};$hashes=@{}
  foreach($file in $files.Keys){$hashes[$file]=(Get-FileHash $files[$file]).Hash;Guest @('copyFileFromHostToGuest',$VMX,$files[$file],"$guest\$file");if($file -ne 'open-key-adapter.exe'){Copy-Item $files[$file] "$bin\$file" -Force}}
  & "$bin\open-key-adapter.exe" "$bin\KxNt.dll" "$archive\$arch-host.txt"
  if($LASTEXITCODE){throw 'Native delegation probe failed'}
  $hostText=[IO.File]::ReadAllText("$archive\$arch-host.txt");Check $hostText $bin 1
  Guest @('runProgramInGuest',$VMX,"$guest\open-key-adapter.exe","$guest\KxNt.dll","$guest\open-key-adapter.txt")
  Guest @('copyFileFromGuestToHost',$VMX,"$guest\open-key-adapter.txt","$archive\$arch-vm.txt")
  $text=[IO.File]::ReadAllText("$archive\$arch-vm.txt");Check $text $guest 0
  $sources=@{};foreach($file in @('KexDll/openkeyex.c','KexDll/KexDll.def','KxNt/forwards.c','00-Common-Headers/KexDll.h','tests/kxnt_open_key_adapter_probe.c','tests/run_kxnt_open_key_adapter.ps1')){$sources[$file]=(Get-FileHash "$root\$file").Hash}
  $rows+=[pscustomobject]@{Architecture=$arch;VMX=$VMX;GuestUser=$GuestUser;Hashes=$hashes;SourceSHA256=$sources;HostOutput=$hostText;Output=$text;Scope='OpenOptions=0 / REG_OPTION_OPEN_LINK on NT 6.0, preserving native OBJECT_ATTRIBUTES; native delegation when available; other nonzero options explicitly unsupported on NT 6.0; scratch load, no normal IFEO or backup/restore support'}
  Write-Host "$arch registry ordinary open: PASS"
 }
 $state='Passed'
} finally {[pscustomobject]@{State=$state;RunName=$RunName;Results=$rows}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$archive\receipt.json"}
