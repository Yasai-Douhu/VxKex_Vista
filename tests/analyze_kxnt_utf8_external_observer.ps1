param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output,[switch]$RequirePhaseIdentity,[switch]$RequireSteadyState)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve earlier analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
if(!$r.Utf8Observer -or $r.Utf8ResourceSnapshot -or $r.ClientVista -or $r.EventTrace -or !$r.RuntimeSuite -or $r.State -notin @('Measured','Failed')){throw 'Require explicit external observation on Server clone'}
if($r.DriverOutput -notmatch 'OSVersion=6\.0\.[0-9]+ ProductType=3 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored' -or @([regex]::Matches($r.DriverOutput,'(?m)^PASS owned IFEO key absent after cleanup')).Count -ne 4){throw 'Native product or cleanup not verified'}
$unexpected=@($r.DriverOutput -split '\r?\n'|Where-Object {$_ -match '^FAIL ' -and $_ -notmatch '^FAIL (registered static imports and detailed comparisons|native lookup-only resource control completed) Error='})
if($unexpected.Count -or $r.DebuggerFiles.Count -ne 2 -or $r.EventResults.Count -ne 4){throw 'Non-resource deployment failure or incomplete observation'}
$results=@()
foreach($arch in @('x86','x64')){
 $suite=@($r.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq 'utf8'})
 if($suite.Count -ne 1 -or $suite[0].Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Actual static binding missing'}
 $actual=@($suite[0].Output -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
 $expected=@($suite[0].ReferenceOutput -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
 if($actual.Count -ne 4893 -or ($actual -join "`n") -cne ($expected -join "`n")){throw 'Conversion semantics differ from native reference'}
 foreach($mode in @('adapter','control')){
  $observation=@($r.EventResults|Where-Object {$_.Architecture -eq $arch -and $_.Mode -eq $mode})
  if($observation.Count -ne 1){throw 'Missing unique observation'}
  $text=$observation[0].Output
  $bits=if($arch -eq 'x64'){64}else{32}
  if($text -notmatch "ObserverBits=$bits KexDllLoaded=0 OS=6\.0 ProductType=3" -or $text -notmatch 'SameBitness=1 Observed=1 TargetWait=00000000 TargetExit=0000000[01]'){throw 'Observer did not read a naturally exiting same-bitness target'}
  $detail=if($mode -eq 'adapter'){$suite[0].Output}else{$suite[0].LookupControlOutput}
  $pidMatch=[regex]::Match($text,'KXNT_TARGET_PID=([0-9a-f]+)');$pidValue=[Convert]::ToUInt32($pidMatch.Groups[1].Value,16)
  if($detail -notmatch "ExternalObservationReady=1 PID=$pidValue\b"){throw 'Observation not linked to measured target'}
  $loader=[regex]::Match($text,'ExternalLoaderLock PEB=([0-9A-Fa-f]+) Lock=([0-9A-Fa-f]+) Semaphore=([0-9A-Fa-f]+) LockCount=-1 Recursion=0 NativeOwner=1 PrivateLayout=NT6-diagnostic')
  if(!$loader.Success -or $text -notmatch '(?i)ExternalNativeModule Base=[0-9a-f]+ Size=\d+ Path=C:\\Windows\\(System32|SysWOW64)\\ntdll\.dll Offset=[0-9a-f]+'){throw 'Native loader owner not verified'}
  $semaphore=[Convert]::ToUInt64($loader.Groups[3].Value,16);$objectIdentity=$null
  if(!$semaphore){if($text -notmatch 'ExternalSemaphore Absent=1'){throw 'Missing explicit absence observation'}}else{
   $handle=[regex]::Match($text,"ExternalHandle PID=$pidValue Handle=([0-9a-f]+) Object=([0-9A-Fa-f]+) Access=([0-9a-f]+) LoaderSemaphore=1")
   if(!$handle.Success -or [Convert]::ToUInt64($handle.Groups[1].Value,16) -ne $semaphore -or $text -notmatch 'ExternalSemaphore Type=Event Status=00000000 DuplicateClosed=1'){throw 'Remote semaphore type/identity not verified'}
   $objectIdentity=$handle.Groups[2].Value
  }
  $calls=if($mode -eq 'adapter'){16000}else{8000};$deltas=@()
  foreach($phase in @(0,1)){
   $m=[regex]::Match($detail,"Parallel=4 Phase=$phase Calls=$calls Errors=0 HandleDelta=(-?\d+)")
   if(!$m.Success){throw 'Parallel conversion/lookup failed or phase missing'}
   $deltas+=[int]$m.Groups[1].Value
  }
  $phaseStates=@()
  if($RequirePhaseIdentity){
   foreach($phase in @(0,1)){
    $m=[regex]::Match($detail,"InlineLoaderState Phase=$phase BeforeLock=([0-9A-Fa-f]+) BeforeSemaphore=([0-9A-Fa-f]+) BeforeRead=1 AfterLock=([0-9A-Fa-f]+) AfterSemaphore=([0-9A-Fa-f]+) AfterRead=1 BeforeHandles=(\d+) AfterHandles=(\d+)")
    if(!$m.Success -or [Convert]::ToUInt64($m.Groups[1].Value,16) -ne [Convert]::ToUInt64($loader.Groups[2].Value,16) -or $m.Groups[1].Value -cne $m.Groups[3].Value){throw 'Inline phase lock differs from externally owned native lock'}
    $beforeSem=[Convert]::ToUInt64($m.Groups[2].Value,16);$afterSem=[Convert]::ToUInt64($m.Groups[4].Value,16)
    if(($phase -eq 1 -and $afterSem -ne $semaphore) -or ($afterSem -ne 0 -and $afterSem -ne $semaphore) -or ($beforeSem -ne 0 -and $beforeSem -ne $semaphore) -or [int]$m.Groups[6].Value-[int]$m.Groups[5].Value -ne $deltas[$phase]){throw 'Inline state/count inconsistent with measurement or remote semaphore'}
    $phaseStates+=[pscustomobject]@{Phase=$phase;BeforeSemaphore=$beforeSem;AfterSemaphore=$afterSem;BeforeHandles=[int]$m.Groups[5].Value;AfterHandles=[int]$m.Groups[6].Value;NewNativeEventVerified=($beforeSem -eq 0 -and $afterSem -ne 0 -and $deltas[$phase] -eq 1 -and $null -ne $objectIdentity)}
   }
   if($phaseStates[0].AfterSemaphore -ne $phaseStates[1].BeforeSemaphore){throw 'Native semaphore changed between phases'}
   $targetCount=[regex]::Match($text,'ExternalTargetHandleCount=(\d+)')
   if(!$targetCount.Success -or [int]$targetCount.Groups[1].Value -ne $phaseStates[1].AfterHandles){throw 'Target handle count changed before external read'}
  }
  $steady=$null
  if($RequireSteadyState){
   $workers=[regex]::Match($detail,'SteadyWorkers=4 CacheWarmFromLegacyCalls=1 IDs=(\d+),(\d+),(\d+),(\d+)')
   if(!$workers.Success -or @(@(1..4|ForEach-Object {$workers.Groups[$_].Value})|Select-Object -Unique).Count -ne 4){throw 'Four distinct surviving workers not recorded'}
   $phases=@([regex]::Matches($detail,'(?m)^SteadyPhase=(\d+) Calls=(\d+) Errors=0 BeforeHandles=(\d+) AfterHandles=(\d+) HandleDelta=0\r?$'))
   if($phases.Count -ne 8){throw 'Steady resource phase failed or missing'}
   foreach($phase in @(0..7)){
    $m=$phases[$phase]
    if([int]$m.Groups[1].Value -ne $phase -or [int]$m.Groups[2].Value -ne $calls -or $m.Groups[3].Value -cne $m.Groups[4].Value -or $m.Groups[3].Value -cne $phases[0].Groups[3].Value){throw 'Steady phase/count inconsistent or drift between phases'}
   }
   $life=[regex]::Match($detail,'SteadyLifecycle BeforeHandles=(\d+) AfterHandles=(\d+) Delta=(-?\d+) OwnedHandlesClosed=12')
   if(!$life.Success -or [int]$life.Groups[2].Value-[int]$life.Groups[1].Value -ne [int]$life.Groups[3].Value){throw 'Owned worker resources not closed or lifecycle count inconsistent'}
   $total=$calls*8
   if($detail -notmatch "SteadyPhases=8 TotalCalls=$total Failures=0 Result=PASS"){throw 'Steady calls or worker exit failed'}
   $steady=[pscustomobject]@{Phases=8;TotalCalls=$total;AllPhaseHandleDeltas=0;Workers=4;OwnedHandlesClosed=12;ThreadLifecycleDelta=[int]$life.Groups[3].Value;Passed=$true;Limits='Caches already populated by legacy calls; measures conversions with existing workers, not cold initialization or thread lifecycle'}
  }
  $results+=[pscustomobject]@{Architecture=$arch;Mode=$mode;TargetPID=$pidValue;ColdDelta=$deltas[0];WarmDelta=$deltas[1];LoaderSemaphore=$semaphore;EventObjectIdentity=$objectIdentity;PairResourceGatePassed=$suite[0].Passed;PhaseStates=$phaseStates;SteadyState=$steady;Measured=$true}
 }
}
$lifecycleComparisons=@()
if($RequireSteadyState){foreach($arch in @('x86','x64')){
 $adapter=@($results|Where-Object {$_.Architecture -eq $arch -and $_.Mode -eq 'adapter'})[0]
 $control=@($results|Where-Object {$_.Architecture -eq $arch -and $_.Mode -eq 'control'})[0]
 $lifecycleComparisons+=[pscustomobject]@{Architecture=$arch;AdapterDelta=$adapter.SteadyState.ThreadLifecycleDelta;ControlDelta=$control.SteadyState.ThreadLifecycleDelta;Matched=($adapter.SteadyState.ThreadLifecycleDelta -eq $control.SteadyState.ThreadLifecycleDelta);Scope='Counts include startup/exit outside surviving-worker API phases; ownership of additional handles not inferred'}
}}
[pscustomobject]@{State='Measured';SourceReceiptState=$r.State;PhaseIdentityRequired=[bool]$RequirePhaseIdentity;SteadyStateRequired=[bool]$RequireSteadyState;ReceiptSHA256=(Get-FileHash $Receipt).Hash;Results=$results;LifecycleComparisons=$lifecycleComparisons;Scope='External read after parallel measurements, with a test-only 3s hold and optional inline NT6 semaphore/count state. NewNativeEventVerified identifies the event in the recorded phase; optional steady-state gate measures warmed APIs with surviving workers. LifecycleComparisons keeps startup/exit differences separate and unresolved. No allocation stack or attribution of older runs, balanced hidden allocations or general resource completion. Legacy resource gates remain unchanged.'}|ConvertTo-Json -Depth 7|Set-Content $Output -Encoding UTF8
