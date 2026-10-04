param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve prior analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
if(!$r.ClientVista -or !$r.RuntimeSuite -or $r.Utf8Trace -or $r.EventTrace -or $r.State -ne 'Passed' -or $r.DriverExit -ne 0 -or $r.SuiteResults.Count -ne 4){throw 'Require full Vista ordinary-runtime success'}
if($r.DriverOutput -notmatch 'OSVersion=6\.0\.[0-9]+ ProductType=1 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -match '(?m)^FAIL ' -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored'){throw 'Native product or lifecycle failed'}
$results=@()
foreach($arch in @('x86','x64')){foreach($kind in @('utf8','silent-exit')){
 $items=@($r.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq $kind})
 if($items.Count -ne 1 -or !$items[0].Passed -or $items[0].Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Missing behavior or main binding'}
 $text=$items[0].Output;$reference=$items[0].ReferenceOutput
 if($text -notmatch 'Failures=0' -or $text -match 'Result=FAIL'){throw 'Behavior failed'}
 if($kind -eq 'utf8'){
  $actual=@($text -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
  $expected=@($reference -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
  if($actual.Count -ne 4893 -or ($actual -join "`n") -cne ($expected -join "`n")){throw 'Native UTF status/buffer/TLS/scalar comparison differs'}
  $control=$items[0].LookupControlOutput
  $cold=[regex]::Match($text,'Parallel=4 Phase=0 Calls=16000 Errors=0 HandleDelta=(-?\d+)')
  $native=[regex]::Match($control,'Parallel=4 Phase=0 Calls=8000 Errors=0 HandleDelta=(-?\d+)')
  if(!$cold.Success -or !$native.Success -or $cold.Groups[1].Value -ne $native.Groups[1].Value -or $text -notmatch 'Phase=1 Calls=16000 Errors=0 HandleDelta=0' -or $control -notmatch 'Phase=1 Calls=8000 Errors=0 HandleDelta=0' -or $control -notmatch 'Failures=0 Result=CONTROL'){throw 'UTF native-controlled resource gate failed'}
 }else{
  $actual=@($text -split '\r?\n'|Where-Object {$_ -match '^Case='})
  $expected=@($reference -split '\r?\n'|Where-Object {$_ -match '^Case='}|ForEach-Object {if($_ -match '^Case=self(?: |-)'){$_ -replace 'Status=00000000','Status=c00000bb'}else{$_}})
  if($actual.Count -ne 9 -or ($actual -join "`n") -cne ($expected -join "`n") -or $text -notmatch 'RepeatCalls=1000 HandleDelta=0' -or $text -notmatch 'Failures=0 ReportingSupported=0'){throw 'SilentExit explicit unsupported semantics differ'}
 }
 $results+=[pscustomobject]@{Architecture=$arch;Probe=$kind;Passed=$true;ReferenceSHA256=$items[0].ReferenceSHA256;FixtureSHA256=$items[0].FixtureSHA256}
}}
if([regex]::Matches($r.DriverOutput,'PASS installed KexDll/KxNt byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS installed package-root DLL byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS unregistered static import fails before main').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 4){throw 'Install byte, negative or teardown gate incomplete'}
[pscustomobject]@{State='Passed';ReceiptSHA256=(Get-FileHash $Receipt).Hash;Results=$results;Scope='Vista client x64/WOW64 normal IFEO UTF functional/native-controlled resource run, and SilentExit explicit NOT_SUPPORTED with ordinary validation. Not WER reporting or resolution of retained Server resource failures.'}|ConvertTo-Json -Depth 5|Set-Content $Output -Encoding UTF8
