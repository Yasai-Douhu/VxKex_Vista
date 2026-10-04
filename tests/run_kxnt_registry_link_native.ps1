param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [switch]$Adapter
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$allowed='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Next-Parity-Test\Server2008-SetupParity.vmx'
if([IO.Path]::GetFullPath($VMX) -ine $allowed){throw 'Owned registry experiments use only the disposable clone'}
$archive="$root\audit\KxNtParity\registry-link-$RunName"
if(Test-Path $archive){throw 'Preserve old evidence; choose a new run'}
New-Item -ItemType Directory $archive|Out-Null
$vmrun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
$guest="C:\VxKexProbe\RegistryLink-$RunName"
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
$state='Incomplete';$results=@();$source="$PSScriptRoot\kxnt_registry_link_native_probe.c"
function Guest([string[]]$Arguments){& $vmrun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "Guest command failed: $($Arguments[0])"}}
try{
 Guest @('createDirectoryInGuest',$VMX,$guest)
 foreach($arch in @('x86','x64')){
  $suffix=if($arch -eq 'x64'){'\amd64'}else{''};$libSuffix=if($arch -eq 'x64'){'\x64'}else{''}
  $env:PATH="$vc\bin$suffix;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
  $env:INCLUDE="$sdk\Include;$vc\include";$env:LIB="$sdk\Lib$libSuffix;$vc\lib$suffix"
  $bin="$archive\$arch";New-Item -ItemType Directory $bin|Out-Null
  $guestBin="$guest\$arch";Guest @('createDirectoryInGuest',$VMX,$guestBin)
  $exe="$bin\RegistryLinkNative-$arch.exe";$files=@{}
  & cl.exe /nologo /W4 /O1 /MT "/Fo$archive\probe-$arch.obj" "/Fe$exe" $source /link advapi32.lib /SUBSYSTEM:CONSOLE,6.0 *> "$archive\build-$arch.txt"
  if($LASTEXITCODE){throw "$arch native probe build failed"}
  if($Adapter){
   $package=if($arch -eq 'x86'){"$root\Installer\Kex32"}else{"$root\Installer"}
   foreach($name in @('KxNt.dll','KexDll.dll')){Copy-Item "$package\$name" "$bin\$name";Guest @('copyFileFromHostToGuest',$VMX,"$bin\$name","$guestBin\$name");$files[$name]=(Get-FileHash "$bin\$name").Hash}
   & $exe "$archive\host-$arch.txt" "$bin\KxNt.dll"
  }else{& $exe "$archive\host-$arch.txt"}
  $hostExit=$LASTEXITCODE
  Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guestBin\RegistryLinkNative-$arch.exe")
  [string[]]$launch=@('runProgramInGuest',$VMX,"$guestBin\RegistryLinkNative-$arch.exe","$guestBin\native-$arch.txt")
  if($Adapter){$launch+="$guestBin\KxNt.dll"}
  & $vmrun -T ws -gu $GuestUser -gp $GuestPassword @launch
  $guestExit=$LASTEXITCODE
  Guest @('copyFileFromGuestToHost',$VMX,"$guestBin\native-$arch.txt","$archive\native-$arch.txt")
  $hostLog=[IO.File]::ReadAllText("$archive\host-$arch.txt");$guestLog=[IO.File]::ReadAllText("$archive\native-$arch.txt")
  $loaded=[int][bool]$Adapter
  $passed=$hostExit -eq 0 -and $guestExit -eq 0 -and $hostLog -match "NativeEx=1 KexDllLoaded=$loaded" -and $guestLog -match "NativeEx=0 KexDllLoaded=$loaded" -and $hostLog -match 'Failures=0 Result=PASS' -and $guestLog -match 'Failures=0 Result=PASS' -and [regex]::Matches($hostLog,'Check=open-link-semantics Pass=1').Count -eq 8 -and [regex]::Matches($guestLog,'Check=open-link-semantics Pass=1').Count -eq $(if($Adapter){8}else{4}) -and $hostLog -match 'Check=owned-base-absent Pass=1' -and $guestLog -match 'Check=owned-base-absent Pass=1'
  if($Adapter){$passed=$passed -and $hostLog -match [regex]::Escape("AdapterProvider=$bin\KxNt.dll Implementation=$bin\KexDll.dll") -and $guestLog -match [regex]::Escape("AdapterProvider=$guestBin\KxNt.dll Implementation=$guestBin\KexDll.dll")}
  $results+=[pscustomobject]@{Architecture=$arch;Passed=$passed;CandidateSHA256=$files;ExeSHA256=(Get-FileHash $exe).Hash;HostExit=$hostExit;GuestExit=$guestExit;HostOutput=$hostLog;GuestOutput=$guestLog}
  if(!$passed){throw "$arch native registry-link semantics or cleanup failed"}
 }
 $state='Passed'
}finally{
 [pscustomobject]@{State=$state;VMX=$VMX;Adapter=[bool]$Adapter;SourceSHA256=(Get-FileHash $source).Hash;RunnerSHA256=(Get-FileHash $PSCommandPath).Hash;Results=$results;Scope='Owned link semantics using native controls and optional scratch candidate. No full backup/restore privilege, pointer/ACL equivalence or Vista client integration is proven by this fixture.'}|ConvertTo-Json -Depth 6|Set-Content "$archive\receipt.json" -Encoding UTF8
}
