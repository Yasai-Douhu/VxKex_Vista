param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [switch]$Suite,
 [switch]$EventTrace,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
if($EventTrace -and !$Suite){throw 'EventTrace uses the static-import suite fixtures; also specify Suite'}
$root=Split-Path $PSScriptRoot
$allowed='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Next-Parity-Test\Server2008-SetupParity.vmx'
if([IO.Path]::GetFullPath($VMX) -ine $allowed){throw 'This installer-changing runner only accepts the disposable clone, never the user VM'}
$archive="$root\audit\KxNtParity\ifeo-$RunName";$guest='C:\VxKexProbe\KxNtIfeo'
if(Test-Path $archive){throw 'Use a fresh RunName to retain earlier evidence'}
New-Item -ItemType Directory $archive|Out-Null
$state='Incomplete';$results=@();$suiteResults=@();$eventResults=@();$debuggerFiles=@();$files=@();$sources=@{}
$kinds=@('processor-feature','domain','device-family','persisted-state','sid-package','sid-capability','membership','compare','file-information','alert','performance','srw')
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
try {
 foreach($source in @('tests/kxnt_open_key_adapter_probe.c','tests/kxnt_ifeo_deployment_probe.c','tests/kxnt_ifeo_imports.def','tests/build_kxnt_ifeo_probe.ps1','tests/run_kxnt_ifeo_vm.ps1')){$sources[$source]=(Get-FileHash "$root\$source").Hash}
 if($Suite){
  foreach($source in @('tests/kxnt_ifeo_suite_provider.h','tests/kxnt_ifeo_suite_imports.def','tests/kxnt_utf8_trace.h')){$sources[$source]=(Get-FileHash "$root\$source").Hash}
  if($EventTrace){$sources['tests/kxnt_ifeo_event_trace.cdb']=(Get-FileHash "$PSScriptRoot\kxnt_ifeo_event_trace.cdb").Hash}
  foreach($kind in $kinds){$source=if($kind -like 'sid-*'){'sid_class'}else{$kind.Replace('-','_')};$source="tests/kxnt_${source}_probe.c";$sources[$source]=(Get-FileHash "$root\$source").Hash}
 }
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
  if($Suite){foreach($kind in $kinds){$image="$root\audit\KxNtParity\IfeoSuite\$arch\KxNtIfeo-$kind-$arch.exe";Guest @('copyFileFromHostToGuest',$VMX,$image,"$guest\KxNtIfeo-$kind-$arch.exe")}}
 }
 Guest @('copyFileFromHostToGuest',$VMX,"$root\audit\KxNtParity\x64\ifeo-deployment.exe","$guest\deployment.exe")
 if($EventTrace){
  $extension='C:\Program Files\Debugging Tools for Windows (x64)\winxp\wow64exts.dll'
  Guest @('copyFileFromHostToGuest',$VMX,$extension,"$guest\wow64exts.dll")
  Guest @('copyFileFromHostToGuest',$VMX,"$PSScriptRoot\kxnt_ifeo_event_trace.cdb","$guest\event-trace.cdb")
  $debuggerFiles+=[pscustomobject]@{Name='wow64exts.dll';SHA256=(Get-FileHash $extension).Hash}
  foreach($file in @(@('C:\VxKexProbe\Wow64\cdb.exe','cdb.exe'),@('C:\Windows\SysWOW64\ntdll.dll','native-ntdll-x86.dll'),@('C:\Windows\System32\ntdll.dll','native-ntdll-x64.dll'))){
   Guest @('copyFileFromGuestToHost',$VMX,$file[0],"$archive\$($file[1])")
   $debuggerFiles+=[pscustomobject]@{Name=$file[1];GuestPath=$file[0];SHA256=(Get-FileHash "$archive\$($file[1])").Hash}
  }
  if((Get-FileHash "$archive\native-ntdll-x86.dll").Hash -ne 'A3D767B53F36E97DFEFCAD305AE050C98D5282D0107B7D51A36825C25F544506'){throw 'Internal CDB offsets are diagnostic-only and require this exact verified clone ntdll image'}
 }
 # Capture a failing driver too, including cleanup results, before interpreting exit.
 [string[]]$driverArgs=@('runProgramInGuest',$VMX,"$guest\deployment.exe");if($EventTrace){$driverArgs+='--event-trace'}elseif($Suite){$driverArgs+='--suite'}
 & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @driverArgs
 $driverExit=$LASTEXITCODE
 Guest @('copyFileFromGuestToHost',$VMX,"$guest\deployment.txt","$archive\deployment.txt")
 $driver=[IO.File]::ReadAllText("$archive\deployment.txt")
 foreach($arch in @(if(!$EventTrace){'x86';'x64'})){
  & $VMRun -T ws -gu $GuestUser -gp $GuestPassword copyFileFromGuestToHost $VMX "$guest\applied-$arch.txt" "$archive\applied-$arch.txt"
  if(!$LASTEXITCODE){
   $text=[IO.File]::ReadAllText("$archive\applied-$arch.txt")
   $providerPattern=if($arch -eq 'x86'){'Provider=C:\\VxKex\\Kex32\\KxNt.dll'}else{'Provider=C:\\Windows\\System32\\KxNt.dll'}
   $passed=$text -match $providerPattern -and $text -match 'StaticImportEqual=1 EarlyKexDllLoaded=1' -and $text -match 'Implementation=C:\\Windows\\(system32|System32|SysWOW64)\\KexDll.dll NativePresent=0 AliasEqual=1' -and [regex]::Matches($text,'(?m)^Case=').Count -eq 140 -and $text -notmatch 'Match=0' -and $text -match 'Repeated=1000 HandleDelta=0' -and [regex]::Matches($text,'(?m)^Unsupported=.*Status=c00000bb OutputUntouched=1').Count -eq 8 -and $text -match 'Failures=0 Result=PASS'
   $results+=[pscustomobject]@{Architecture=$arch;Passed=$passed;FixtureSHA256=(Get-FileHash "$root\audit\KxNtParity\$arch\KxNtIfeoOpen-$arch.exe").Hash;Output=$text}
  }
 }
 if($EventTrace){foreach($kind in @('compare','srw')){
  foreach($file in @("applied-$kind-x86.txt","bindings-$kind-x86.txt","event-$kind-x86.log")){Guest @('copyFileFromGuestToHost',$VMX,"$guest\$file","$archive\$file")}
  $text=[IO.File]::ReadAllText("$archive\applied-$kind-x86.txt");$binding=[IO.File]::ReadAllText("$archive\bindings-$kind-x86.txt");$trace=[IO.File]::ReadAllText("$archive\event-$kind-x86.log")
  $measured=$text -match 'Result=(PASS|FAIL)' -and $binding -match 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS' -and $trace -match 'KXNT_TARGET_PID=[0-9a-f]+' -and $trace -match '(?m)^KXNT_EVENT_CREATE32\r?$' -and [regex]::Matches($trace,'(?m)^KXNT_RESOURCE_PHASE\r?$').Count -eq 2
  $eventResults+=[pscustomobject]@{Architecture='x86';Probe=$kind;Measured=$measured;ProbePassed=$text -match 'Result=PASS';Output=$text;Bindings=$binding;Trace=$trace;FixtureSHA256=(Get-FileHash "$root\audit\KxNtParity\IfeoSuite\x86\KxNtIfeo-$kind-x86.exe").Hash;DebuggerMayPerturb=$true}
 }}
 if($Suite -and !$EventTrace){foreach($arch in @('x86','x64')){foreach($kind in $kinds){
  $copied=$true
  foreach($type in @('applied','bindings')){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword copyFileFromGuestToHost $VMX "$guest\$type-$kind-$arch.txt" "$archive\$type-$kind-$arch.txt";if($LASTEXITCODE){$copied=$false}}
  if(!$copied){$suiteResults+=[pscustomobject]@{Architecture=$arch;Probe=$kind;Passed=$false;Error='Missing actual output or main-process static binding evidence'};continue}
  $text=[IO.File]::ReadAllText("$archive\applied-$kind-$arch.txt");$binding=[IO.File]::ReadAllText("$archive\bindings-$kind-$arch.txt")
  $provider=if($arch -eq 'x86'){'C:\VxKex\Kex32\KxNt.dll'}else{'C:\Windows\System32\KxNt.dll'}
  $passed=$text -match 'Result=PASS' -and $text -notmatch 'Failures=[1-9]|Result=FAIL' -and $binding -match [regex]::Escape("Provider=$provider") -and $binding -match 'Implementation=C:\\Windows\\System32\\KexDll\.dll' -and $binding -match 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS' -and [regex]::Matches($binding,'(?m)^Import=.*Equal=1 Owner=.+').Count -eq 26
  $referenceHash=$null
  switch($kind){
   'processor-feature' {$passed=$passed -and $text -match 'ValidFeatureChecks=64 InvalidFeatureChecks=5 Failures=0'}
   'domain' {$passed=$passed -and $text -match 'AllocationFreeCycles=2000 Failures=0'}
   'device-family' {$passed=$passed -and $text -match 'OptionalOutputCombinations=8 Failures=0' -and $text -match 'SpoofedVersionCases=3'}
   'persisted-state' {$passed=$passed -and $text -match 'PersistedStateCases=16 Failures=0'}
   {$_ -like 'sid-*'} {
    $referenceFile="$root\docs\validation\kxnt-$kind-reference.json";$reference=Get-Content $referenceFile -Raw|ConvertFrom-Json
    $actual=@($text -split '\r?\n'|Where-Object {$_ -match '^Rev=|^NULL |^GuardBytes='})
    $expected=@($reference.Output|Where-Object {$_ -match '^Rev=|^NULL |^GuardBytes='})
    $passed=$passed -and $text -match 'SidClassificationCases=660' -and $actual.Count -eq 678 -and ($actual -join "`n") -ceq ($expected -join "`n")
    $referenceHash=(Get-FileHash $referenceFile).Hash
   }
   'membership' {$passed=$passed -and $text -match 'StressIterations=2000 HandleDelta=0' -and $text -match 'MembershipCases=[1-9][0-9]* Failures=0'}
   'compare' {
    $before=@($text -split '\r?\n'|Where-Object {$_ -match '^Diagnostic before '}|ForEach-Object {$_ -replace '^Diagnostic before ',''})
    $after=@($text -split '\r?\n'|Where-Object {$_ -match '^Diagnostic after '}|ForEach-Object {$_ -replace '^Diagnostic after ',''})
    $passed=$passed -and $before.Count -gt 0 -and $after.Count -gt 0 -and !(Compare-Object $before $after) -and $text -match 'ConcurrentCalls=800 HandleDelta=0'
   }
   'file-information' {$passed=$passed -and $text -match 'FileInformationCases=96 Failures=0' -and [regex]::Matches($text,'RepeatCalls=1000 HandleDelta=0').Count -eq 2}
   'alert' {$passed=$passed -and [regex]::Matches($text,'ChurnThreads=4096 EarlyErrors=0 LaterErrors=0 HandleDelta=0').Count -eq 2 -and [regex]::Matches($text,'TerminatedWaiterTest Exit=0 PASS').Count -eq 2}
   'performance' {$passed=$passed -and $text -match 'UnalignedOutputCases=16 Failures=0' -and $text -match 'Threads=4 CounterCalls=4000 FrequencyCalls=4000 InvalidOutputCases=10 Failures=0'}
   'srw' {$passed=$passed -and $text -match 'MixedOperations=4000 Threads=4 Writes=2000 HandleDelta=0' -and $text -match 'ConditionShared=0 Exit=0' -and $text -match 'ConditionShared=1 Exit=0'}
  }
  $bin="$root\audit\KxNtParity\IfeoSuite\$arch"
  $suiteResults+=[pscustomobject]@{Architecture=$arch;Probe=$kind;Passed=$passed;FixtureSHA256=(Get-FileHash "$bin\KxNtIfeo-$kind-$arch.exe").Hash;ImportTable=[IO.File]::ReadAllText("$bin\$kind-imports.txt");ReferenceSHA256=$referenceHash;Bindings=$binding;Output=$text}
  Write-Host "$arch $kind integration Passed=$passed"
 }}}
 $state='Failed'
 if($Suite -and !$EventTrace -and ($suiteResults.Count -ne 24 -or ($suiteResults|Where-Object {!$_.Passed}))){throw 'Detailed suite failed; preserve actual output, bindings and teardown'}
 if($driverExit -ne 0 -or $driver -match '(?m)^FAIL ' -or $driver -notmatch 'Failures=0 Result=PASS' -or (!$EventTrace -and ($results.Count -ne 2 -or ($results|Where-Object {!$_.Passed})))){throw 'IFEO integration or deployment cleanup failed; preserve raw logs'}
 if($EventTrace){if($eventResults.Count -ne 2 -or ($eventResults|Where-Object {!$_.Measured})){throw 'Incomplete event trace'};$state='Measured'}else{$state='Passed'}
} finally {
 [pscustomobject]@{State=$state;VMX=$VMX;GuestUser=$GuestUser;RunName=$RunName;SourceSHA256=$sources;PackageFiles=$files;DriverSHA256=(Get-FileHash "$root\audit\KxNtParity\x64\ifeo-deployment.exe").Hash;DriverExit=$driverExit;DriverOutput=$driver;Results=$results;SuiteIncluded=[bool]$Suite;SuiteResults=$suiteResults;EventTrace=[bool]$EventTrace;EventResults=$eventResults;DebuggerFiles=$debuggerFiles;Scope='Disposable Server 2008 clone, native x64 and WOW64; real install/KexCfg/AVRF/static ntdll import rewrite/ordinary-open comparison and optional detailed suite with exact import-slot equality; event trace is observation under CDB and never replaces failed resource gates; no native x86 OS, Vista IFEO or UTF resource completion; binding an API is not proof of its behavior unless covered by the executed detailed probe'}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$archive\receipt.json"
}
