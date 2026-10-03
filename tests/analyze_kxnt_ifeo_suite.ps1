param([Parameter(Mandatory=$true)][string]$InitialReceipt,[Parameter(Mandatory=$true)][string]$TracedReceipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop';$root=Split-Path $PSScriptRoot
if(Test-Path $Output){throw 'Retain existing analysis; use a new output path'}
$initial=Get-Content $InitialReceipt -Raw|ConvertFrom-Json;$traced=Get-Content $TracedReceipt -Raw|ConvertFrom-Json
if($initial.State -ne 'Failed' -or $traced.State -ne 'Passed' -or $traced.DriverExit -ne 0 -or $traced.SuiteResults.Count -ne 24 -or ($traced.SuiteResults|Where-Object {!$_.Passed})){throw 'Expected preserved initial failure and complete traced run'}
if([regex]::Matches($traced.DriverOutput,'PASS unregistered static import fails before main').Count -ne 26 -or [regex]::Matches($traced.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 26 -or $traced.DriverOutput -notmatch 'OSVersion=6\.0\.6003 ProductType=3 NativeArchitecture=9 DriverKexDllLoaded=0'){throw 'Missing native controls, teardown or actual NT6 clone identity'}
function Identity([string]$Text,[string]$Phase){
 $table=@{}
 foreach($match in [regex]::Matches($Text,"(?m)^Snapshot=$Phase Handle=(\S+) Object=(\S+) Access=(\S+) TypeStatus=00000000 Type=([^\r\n]+)")){
  $key=$match.Groups[1].Value+'/'+$match.Groups[2].Value
  if($table.ContainsKey($key)){throw 'Duplicate handle/object identity'}
  $table[$key]=[pscustomobject]@{Handle=$match.Groups[1].Value;Object=$match.Groups[2].Value;Access=$match.Groups[3].Value;Type=$match.Groups[4].Value}
 }
 $counts=[regex]::Match($Text,"Snapshot=$Phase Entries=(\d+) HandleCount=(\d+)")
 if(!$counts.Success -or $table.Count -ne [int]$counts.Groups[1].Value -or $table.Count -ne [int]$counts.Groups[2].Value){throw 'Snapshot does not cover the counted handles'}
 return $table
}
$resources=@();foreach($arch in @('x86','x64')){
 $row=@($traced.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq 'srw'});if($row.Count -ne 1){throw 'Ambiguous SRW row'}
 $before=Identity $row[0].Output 'srw-before';$after=Identity $row[0].Output 'srw-after'
 $resources+=[pscustomobject]@{Architecture=$arch;BeforeCount=$before.Count;AfterCount=$after.Count;Added=@($after.Keys|Where-Object {!$before.ContainsKey($_)}|ForEach-Object {$after[$_]});Removed=@($before.Keys|Where-Object {!$after.ContainsKey($_)}|ForEach-Object {$before[$_]});SameIdentities=@(Compare-Object @($before.Keys) @($after.Keys)).Count -eq 0}
}
$sid=@();foreach($row in @($initial.SuiteResults|Where-Object {$_.Probe -like 'sid-*'})){
 $path="$root\docs\validation\kxnt-$($row.Probe)-reference.json";$reference=Get-Content $path -Raw|ConvertFrom-Json
 $actual=@($row.Output -split '\r?\n'|Where-Object {$_ -match '^Rev=|^NULL |^GuardBytes='});$expected=@($reference.Output|Where-Object {$_ -match '^Rev=|^NULL |^GuardBytes='})
 $sid+=[pscustomobject]@{Architecture=$row.Architecture;Probe=$row.Probe;Rows=$actual.Count;NativeValueRowsEqual=($actual -join "`n") -ceq ($expected -join "`n");ReferenceSHA256=(Get-FileHash $path).Hash;OriginalReceiptPassed=$row.Passed}
}
$compare=@($initial.SuiteResults|Where-Object {$_.Architecture -eq 'x86' -and $_.Probe -eq 'compare'})[0].Output
$before=@{};$after=@{}
foreach($phase in @('before','after')){foreach($m in [regex]::Matches($compare,"(?m)^Diagnostic $phase Handle=(\S+) Object=(\S+) Type=([^\r\n]+)")){$table=if($phase -eq 'before'){$before}else{$after};$table[$m.Groups[1].Value+'/'+$m.Groups[2].Value]=[pscustomobject]@{Handle=$m.Groups[1].Value;Object=$m.Groups[2].Value;Type=$m.Groups[3].Value}}}
[pscustomobject]@{InitialReceiptSHA256=(Get-FileHash $InitialReceipt).Hash;TracedReceiptSHA256=(Get-FileHash $TracedReceipt).Hash;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;InitialState=$initial.State;InitialDriverExit=$initial.DriverExit;InitialRejectedRows=@($initial.SuiteResults|Where-Object {!$_.Passed}|Select-Object Architecture,Probe);InitialSidReferenceCheck=$sid;InitialCompareAdded=@($after.Keys|Where-Object {!$before.ContainsKey($_)}|ForEach-Object {$after[$_]});TracedState=$traced.State;TracedPassedRows=$traced.SuiteResults.Count;StaticBindingsPerImage=26;NegativeControls=26;OwnedProfileCleanup=26;TracedSrwIdentities=$resources;Limits='Initial WOW64 compare and uninstrumented SRW resource gates failed. Later success does not attribute or repair those Events. SRW snapshots may change timing and loader contention. UTF conversions/NtWriteFile/SilentExit behavior is not tested by binding them. No Vista-client normal IFEO or native x86 OS proof.'}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $Output
