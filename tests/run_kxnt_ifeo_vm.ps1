param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$allowed='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Next-Parity-Test\Server2008-SetupParity.vmx'
if([IO.Path]::GetFullPath($VMX) -ine $allowed){throw 'This installer-changing runner only accepts the disposable clone, never the user VM'}
$archive="$root\audit\KxNtParity\ifeo-$RunName";$guest='C:\VxKexProbe\KxNtIfeo'
if(Test-Path $archive){throw 'Use a fresh RunName to retain earlier evidence'}
New-Item -ItemType Directory $archive|Out-Null
$state='Incomplete';$results=@();$files=@();$sources=@{}
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
try {
 foreach($source in @('tests/kxnt_open_key_adapter_probe.c','tests/kxnt_ifeo_deployment_probe.c','tests/kxnt_ifeo_imports.def','tests/build_kxnt_ifeo_probe.ps1','tests/run_kxnt_ifeo_vm.ps1')){$sources[$source]=(Get-FileHash "$root\$source").Hash}
 # Fixed guest fixture must be absent. No old evidence is adopted or overwritten.
 & $VMRun -T ws -gu $GuestUser -gp $GuestPassword directoryExistsInGuest $VMX $guest
 if($LASTEXITCODE -eq 0){throw 'Guest fixture directory already exists; inspect and preserve it first'}
 Guest @('createDirectoryInGuest',$VMX,$guest)
 Guest @('createDirectoryInGuest',$VMX,"$guest\Package")
 foreach($directory in @(Get-ChildItem "$root\Installer" -Recurse -Directory|Sort-Object {$_.FullName.Length})){
  $relative=$directory.FullName.Substring(("$root\Installer").Length+1)
  Guest @('createDirectoryInGuest',$VMX,"$guest\Package\$relative")
 }
 foreach($file in @(Get-ChildItem "$root\Installer" -Recurse -File)){
  $relative=$file.FullName.Substring(("$root\Installer").Length+1)
  Guest @('copyFileFromHostToGuest',$VMX,$file.FullName,"$guest\Package\$relative")
  $files+=[pscustomobject]@{Path=$relative;SHA256=(Get-FileHash $file.FullName).Hash;Bytes=$file.Length}
 }
 foreach($arch in @('x86','x64')){
  $image="$root\audit\KxNtParity\$arch\KxNtIfeoOpen-$arch.exe"
  Guest @('copyFileFromHostToGuest',$VMX,$image,"$guest\KxNtIfeoOpen-$arch.exe")
 }
 Guest @('copyFileFromHostToGuest',$VMX,"$root\audit\KxNtParity\x64\ifeo-deployment.exe","$guest\deployment.exe")
 # Capture a failing driver too, including cleanup results, before interpreting exit.
 & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\deployment.exe"
 $driverExit=$LASTEXITCODE
 Guest @('copyFileFromGuestToHost',$VMX,"$guest\deployment.txt","$archive\deployment.txt")
 $driver=[IO.File]::ReadAllText("$archive\deployment.txt")
 foreach($arch in @('x86','x64')){
  & $VMRun -T ws -gu $GuestUser -gp $GuestPassword copyFileFromGuestToHost $VMX "$guest\applied-$arch.txt" "$archive\applied-$arch.txt"
  if(!$LASTEXITCODE){
   $text=[IO.File]::ReadAllText("$archive\applied-$arch.txt")
   $providerPattern=if($arch -eq 'x86'){'Provider=C:\\VxKex\\Kex32\\KxNt.dll'}else{'Provider=C:\\Windows\\System32\\KxNt.dll'}
   $passed=$text -match $providerPattern -and $text -match 'StaticImportEqual=1 EarlyKexDllLoaded=1' -and $text -match 'Implementation=C:\\Windows\\(system32|System32|SysWOW64)\\KexDll.dll NativePresent=0 AliasEqual=1' -and [regex]::Matches($text,'(?m)^Case=').Count -eq 140 -and $text -notmatch 'Match=0' -and $text -match 'Repeated=1000 HandleDelta=0' -and [regex]::Matches($text,'(?m)^Unsupported=.*Status=c00000bb OutputUntouched=1').Count -eq 8 -and $text -match 'Failures=0 Result=PASS'
   $results+=[pscustomobject]@{Architecture=$arch;Passed=$passed;FixtureSHA256=(Get-FileHash "$root\audit\KxNtParity\$arch\KxNtIfeoOpen-$arch.exe").Hash;Output=$text}
  }
 }
 $state='Failed'
 if($driverExit -ne 0 -or $driver -match '(?m)^FAIL ' -or $driver -notmatch 'Failures=0 Result=PASS' -or $results.Count -ne 2 -or ($results|Where-Object {!$_.Passed})){throw 'IFEO integration or deployment cleanup failed; preserve raw logs'}
 $state='Passed'
} finally {
 [pscustomobject]@{State=$state;VMX=$VMX;GuestUser=$GuestUser;RunName=$RunName;SourceSHA256=$sources;PackageFiles=$files;DriverSHA256=(Get-FileHash "$root\audit\KxNtParity\x64\ifeo-deployment.exe").Hash;DriverExit=$driverExit;DriverOutput=$driver;Results=$results;Scope='Disposable Server 2008 clone, native x64 and WOW64; real install/KexCfg/AVRF/static ntdll import rewrite/ordinary-open comparison and teardown; no native x86 OS, Vista IFEO, other API integration or UTF resource completion'}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$archive\receipt.json"
}
