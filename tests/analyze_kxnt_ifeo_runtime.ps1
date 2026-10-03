param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Preserve prior evidence; select a new output'}
$r=Get-Content -LiteralPath $Receipt -Raw|ConvertFrom-Json
if(!$r.RuntimeSuite -or $r.SuiteResults.Count -ne 4){throw 'Require the complete runtime suite receipt'}
if([regex]::Matches($r.DriverOutput,'PASS unregistered static import fails before main').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 4 -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored'){throw 'Missing native negative controls or installation teardown'}
$rows=@()
foreach($row in $r.SuiteResults){
 if($row.Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Missing actual main-process IAT proof'}
 $actual=@($row.Output -split '\r?\n');$native=@($row.ReferenceOutput -split '\r?\n')
 if($row.Probe -eq 'silent-exit'){
  $cases=@($actual|Where-Object {$_ -match '^Case='});$expected=@($native|Where-Object {$_ -match '^Case='}|ForEach-Object {if($_ -match '^Case=self(?: |-)'){$_ -replace 'Status=00000000','Status=c00000bb'}else{$_}})
  $equal=$cases.Count -eq 9 -and ($cases -join "`n") -ceq ($expected -join "`n")
  if(!$equal -or !$row.Passed -or $row.Output -notmatch 'RepeatCalls=1000 HandleDelta=0' -or $row.Output -notmatch 'Failures=0 ReportingSupported=0'){throw 'Silent-exit explicit rejection contract failed'}
  $rows+=[pscustomobject]@{Architecture=$row.Architecture;Probe=$row.Probe;NativeHandleValidationEqual=$equal;Cases=$cases.Count;Passed=$row.Passed;ReportingImplemented=$false}
 }elseif($row.Probe -eq 'utf8'){
  $cases=@($actual|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'});$expected=@($native|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
  $equal=$cases.Count -eq 4893 -and ($cases -join "`n") -ceq ($expected -join "`n")
  if(!$equal){throw 'UTF value/status/boundary contract differs from native reference'}
  $phases=@([regex]::Matches($row.Output,'Parallel=4 Phase=(\d) Calls=16000 Errors=(\d+) HandleDelta=(-?\d+)'))
  $controls=@([regex]::Matches($row.LookupControlOutput,'Parallel=4 Phase=(\d) Calls=8000 Errors=(\d+) HandleDelta=(-?\d+)'))
  if($phases.Count -ne 2 -or $controls.Count -ne 2){throw 'Missing full parallel/control observations'}
  $parallel=@();foreach($m in $phases){$parallel+=[pscustomobject]@{Phase=[int]$m.Groups[1].Value;Errors=[int]$m.Groups[2].Value;HandleDelta=[int]$m.Groups[3].Value}}
  $control=@();foreach($m in $controls){$control+=[pscustomobject]@{Phase=[int]$m.Groups[1].Value;Errors=[int]$m.Groups[2].Value;HandleDelta=[int]$m.Groups[3].Value}}
  $resource=$parallel[0].HandleDelta -eq $control[0].HandleDelta -and $parallel[1].HandleDelta -eq 0 -and $control[1].HandleDelta -eq 0
  $rows+=[pscustomobject]@{Architecture=$row.Architecture;Probe=$row.Probe;NativeValueRowsEqual=$equal;Calls=4850;PointerCases=24;OverlapCases=18;ScalarCoverage=1112064;Parallel=$parallel;LookupControl=$control;ResourceGatePassed=$resource;OriginalRowPassed=$row.Passed}
 }else{throw 'Unexpected runtime suite member'}
}
[pscustomobject]@{ReceiptSHA256=(Get-FileHash $Receipt).Hash;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;ReceiptState=$r.State;DriverExit=$r.DriverExit;Results=$rows;Limits='Server 2008 disposable clone ordinary IFEO/AVRF startup only. Native value equality does not replace the UTF resource gate; both failed intervals remain failed. Events are not attributed by count alone. SilentExit verifies explicit unsupported reporting and handle validation, not WER implementation. No Vista IFEO or native x86 OS coverage.'}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $Output -Encoding UTF8
