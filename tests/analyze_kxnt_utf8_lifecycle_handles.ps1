param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$PhaseAnalysis,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve earlier lifecycle analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
$a=Get-Content $PhaseAnalysis -Raw|ConvertFrom-Json
if($a.ReceiptSHA256 -ne (Get-FileHash $Receipt).Hash -or !$a.PhaseIdentityRequired -or !$a.SteadyStateRequired -or $a.Results.Count -ne 4){throw 'Require independently verified phase identity and surviving-worker API phases'}
function Snapshot([string]$text,[int]$stage,[uint32]$targetPid){
 $pattern='(?s)ExternalObservationStage='+$stage+'\r?\n(.*?)(?=ExternalObservationStage=|SameBitness=)'
 $blocks=@([regex]::Matches($text,$pattern))
 if($blocks.Count -ne 1){throw 'Missing unique before/after stage'}
 $block=$blocks[0].Groups[1].Value;$map=@{}
 foreach($m in [regex]::Matches($block,'ExternalHandle PID=(\d+) Handle=([0-9a-f]+) Object=([0-9A-Fa-f]+) Access=([0-9a-f]+) LoaderSemaphore=([01])')){
  if([uint32]$m.Groups[1].Value -ne $targetPid){throw 'Wrong target PID'}
  $handle=[Convert]::ToUInt64($m.Groups[2].Value,16).ToString()
  if($map.ContainsKey($handle)){throw 'Duplicate handle entry'}
  $type=[regex]::Match($block,'(?m)^ExternalType Handle='+[regex]::Escape($m.Groups[2].Value)+' Type=(\w+)\r?$')
  $thread=[regex]::Match($block,'(?m)^ExternalThread Handle='+[regex]::Escape($m.Groups[2].Value)+' ID=(\d+) Start=([0-9A-Fa-f]+) Exit=([0-9a-f]+)\r?$')
  $map[$handle]=[pscustomobject]@{Handle=$handle;ObjectIdentity=$m.Groups[3].Value.ToUpperInvariant();Access=$m.Groups[4].Value;LoaderSemaphore=($m.Groups[5].Value -eq '1');Type=$(if($type.Success){$type.Groups[1].Value}else{$null});Thread=$(if($thread.Success){[pscustomobject]@{ID=$thread.Groups[1].Value;Start=$thread.Groups[2].Value;Exit=$thread.Groups[3].Value}}else{$null})}
 }
 $count=[regex]::Match($block,'ExternalTargetHandleCount=(\d+)');$table=[regex]::Match($block,'ExternalTable Entries=(\d+)')
 if(!$count.Success -or !$table.Success -or [int]$count.Groups[1].Value -ne $map.Count -or [int]$table.Groups[1].Value -ne $map.Count){throw 'Remote table does not match measured process handle count'}
 return [pscustomobject]@{Count=$map.Count;Handles=$map}
}
$results=@()
foreach($item in $a.Results){
 $o=@($r.EventResults|Where-Object {$_.Architecture -eq $item.Architecture -and $_.Mode -eq $item.Mode})
 if($o.Count -ne 1){throw 'Missing actual observer result'}
 $before=Snapshot $o[0].Output 1 $item.TargetPID;$after=Snapshot $o[0].Output 2 $item.TargetPID
 $suite=@($r.SuiteResults|Where-Object {$_.Architecture -eq $item.Architecture -and $_.Probe -eq 'utf8'})[0]
 $detail=if($item.Mode -eq 'adapter'){$suite.Output}else{$suite.LookupControlOutput}
 if($detail -notmatch "ExternalObservationReady=2 PID=$($item.TargetPID)\b"){throw 'After snapshot not linked to same measured process'}
 $life=[regex]::Match($detail,'SteadyLifecycle BeforeHandles=(\d+) AfterHandles=(\d+) Delta=(-?\d+) OwnedHandlesClosed=12')
 if(!$life.Success -or [int]$life.Groups[1].Value -ne $before.Count -or [int]$life.Groups[2].Value -ne $after.Count -or $after.Count-$before.Count -ne $item.SteadyState.ThreadLifecycleDelta){throw 'Snapshot counts differ from worker lifecycle measurements; timing/observer effects remain unverified'}
 $added=@($after.Handles.Values|Where-Object {!$before.Handles.ContainsKey($_.Handle) -or $before.Handles[$_.Handle].ObjectIdentity -cne $_.ObjectIdentity})
 $removed=@($before.Handles.Values|Where-Object {!$after.Handles.ContainsKey($_.Handle) -or $after.Handles[$_.Handle].ObjectIdentity -cne $_.ObjectIdentity})
 if($added.Count-$removed.Count -ne $after.Count-$before.Count){throw 'Identity accounting mismatch'}
 if(@($added|Where-Object {!$_.Type}).Count){throw 'New handle type not verified'}
 $results+=[pscustomobject]@{Architecture=$item.Architecture;Mode=$item.Mode;TargetPID=$item.TargetPID;BeforeCount=$before.Count;AfterCount=$after.Count;Delta=$after.Count-$before.Count;Added=$added;Removed=$removed;Measured=$true}
}
[pscustomobject]@{State='Measured';ReceiptSHA256=(Get-FileHash $Receipt).Hash;PhaseAnalysisSHA256=(Get-FileHash $PhaseAnalysis).Hash;Results=$results;Scope='Same PID before/after surviving-worker lifecycle; exact handle/object identity changes, new object types and count agreement. Toolhelp module snapshots and duplicate/type queries are observations that can affect timing. No attribution of earlier two-handle growth or allocation call stacks; no assumption that an Event is a native cache without separate ownership evidence.'}|ConvertTo-Json -Depth 8|Set-Content $Output -Encoding UTF8
