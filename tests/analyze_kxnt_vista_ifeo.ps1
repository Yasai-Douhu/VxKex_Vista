param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve previous analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
if($r.RuntimeSuite -or $r.ConDrvSuite -or $r.EventTrace){throw 'This analyzer covers ordinary open and the core suite only'}
$expected=if($r.SuiteIncluded){26}else{2}
if($r.State -ne 'Passed' -or !$r.ClientVista -or $r.DriverExit -ne 0){throw 'Vista integration did not pass'}
if($r.DriverOutput -notmatch 'OSVersion=6\.0\.[0-9]+ ProductType=1 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -match '(?m)^FAIL '){throw 'Invalid native Vista baseline or failed deployment'}
foreach($gate in @('elevated operator','fresh deployment required','exact disposable VM marker','install full candidate using real setup','fresh deployment state restored')){
 if($r.DriverOutput -notmatch ('PASS '+[regex]::Escape($gate))){throw "Missing lifecycle gate: $gate"}
}
$results=@()
foreach($arch in @('x86','x64')){
 $items=@($r.Results|Where-Object Architecture -eq $arch)
 if($items.Count -ne 1 -or !$items[0].Passed){throw "Missing Vista architecture: $arch"}
 $text=$items[0].Output
 if([regex]::Matches($text,'(?m)^Case=').Count -ne 280 -or [regex]::Matches($text,'(?m)^Case=.*Option=0 ').Count -ne 140 -or [regex]::Matches($text,'(?m)^Case=.*Option=8 ').Count -ne 140 -or $text -match 'Match=0'){throw 'Native comparison matrix incomplete'}
 if($text -notmatch 'StaticImportEqual=1 EarlyKexDllLoaded=1' -or $text -notmatch 'NativePresent=0 AliasEqual=1' -or $text -notmatch 'Repeated=1000 HandleDelta=0' -or $text -notmatch 'Failures=0 Result=PASS' -or [regex]::Matches($text,'(?m)^Unsupported=.*Status=c00000bb OutputUntouched=1').Count -ne 7){throw 'Binding, fallback, resource or unsupported-option gate missing'}
 $results+=[pscustomobject]@{Architecture=$arch;NativeComparisons=280;OpenOptions=@(0,8);UnsupportedCases=7;Repeated=1000;HandleDelta=0;StaticImportAndEarlyLoad=$true;Passed=$true}
}
if([regex]::Matches($r.DriverOutput,'PASS installed KexDll/KxNt byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS installed package-root DLL byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS unregistered static import fails before main').Count -ne $expected -or [regex]::Matches($r.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne $expected){throw 'Install bytes, negative controls or cleanup incomplete'}
$details=@()
if($r.SuiteIncluded){
 if($r.SuiteResults.Count -ne 24){throw 'Core suite must contain all 24 architecture/probe pairs'}
 foreach($arch in @('x86','x64')){foreach($kind in @('processor-feature','domain','device-family','persisted-state','sid-package','sid-capability','membership','compare','file-information','alert','performance','srw')){
  $items=@($r.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq $kind})
  if($items.Count -ne 1 -or !$items[0].Passed -or $items[0].Output -notmatch 'Result=PASS' -or $items[0].Output -match '(?m)^.*Failures=[1-9]' -or $items[0].Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw "Missing detailed behavior or static binding: $arch $kind"}
  $details+=[pscustomobject]@{Architecture=$arch;Probe=$kind;Passed=$true;FixtureSHA256=$items[0].FixtureSHA256}
 }}
}
[pscustomobject]@{State='Passed';ReceiptSHA256=(Get-FileHash $Receipt).Hash;VMX=$r.VMX;Results=$results;DetailedSuite=$details;Scope='Vista client native x64 and WOW64 ordinary/open-link adapter boundary and, only when DetailedSuite has 24 pairs, executed core behaviors through actual IFEO install, static import, KexCfg and teardown. Not an owned-link creation matrix, ConDrv/UTF suite, native x86 OS or full NtOpenKeyEx extended-option implementation.'}|ConvertTo-Json -Depth 5|Set-Content $Output -Encoding UTF8
