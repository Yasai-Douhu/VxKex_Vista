param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve prior analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
if(!$r.Utf8ResourceSnapshot -or $r.ClientVista -or $r.EventTrace -or $r.State -ne 'Measured' -or $r.DriverExit -ne 0){throw 'Require completed Server-clone snapshot observation, not ordinary-test success'}
if($r.DriverOutput -notmatch 'OSVersion=6\.0\.[0-9]+ ProductType=3 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored' -or $r.DriverOutput -match '(?m)^FAIL '){throw 'Native baseline or cleanup failed'}
function Handles([string]$text,[string]$phase){
 $map=@{}
 foreach($m in [regex]::Matches($text,'(?m)^Snapshot='+[regex]::Escape($phase)+' Handle=([0-9a-f]+) Object=([0-9A-Fa-f]+) Access=([0-9a-f]+) TypeStatus=00000000 Type=(\w+)\r?$')){
  $key=([Convert]::ToUInt64($m.Groups[1].Value,16)).ToString()
  $map[$key]=[pscustomobject]@{Handle=$key;Object=$m.Groups[2].Value;Type=$m.Groups[4].Value;Access=$m.Groups[3].Value}
 }
 if(!$map.Count){throw 'Missing successful handle snapshot'}
 return $map
}
$results=@()
foreach($arch in @('x86','x64')){
 $item=@($r.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq 'utf8'})
 if($item.Count -ne 1 -or !$item[0].Passed -or $item[0].Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Missing actual UTF observation or binding'}
 foreach($mode in @('adapter','lookup-control')){
  $text=if($mode -eq 'adapter'){$item[0].Output}else{$item[0].LookupControlOutput}
  $loaders=@{}
  foreach($m in [regex]::Matches($text,'(?m)^LoaderLockSnapshot=(\w+) PEB=([0-9A-Fa-f]+) Lock=([0-9A-Fa-f]+) Semaphore=([0-9A-Fa-f]+) LockCount=-1 Recursion=0 Owner=(.+) Offset=([0-9a-f]+) PrivateLayout=NT6-diagnostic\r?$')){
   $loaders[$m.Groups[1].Value]=[pscustomobject]@{PEB=$m.Groups[2].Value;Lock=$m.Groups[3].Value;Semaphore=[Convert]::ToUInt64($m.Groups[4].Value,16);Owner=$m.Groups[5].Value;Offset=$m.Groups[6].Value}
  }
  $before=Handles $text 'before';$after=Handles $text 'immediate'
  if(!$loaders.ContainsKey('before') -or $loaders.before.Semaphore -ne 0){throw 'No empty loader semaphore baseline'}
  $born=@($after.Values|Where-Object {!$before.ContainsKey($_.Handle) -or $before[$_.Handle].Object -ne $_.Object})
  if($born.Count -ne 1 -or $born[0].Type -ne 'Event'){throw 'Not exactly one new Event identity'}
  $identity=$born[0]
  foreach($phase in @('immediate','after100ms','after1000ms','warm100ms')){
   if(!$loaders.ContainsKey($phase)){throw 'Missing loader-lock observation'}
   $lock=$loaders[$phase];$handles=Handles $text $phase
   if($lock.Lock -ne $loaders.before.Lock -or $lock.Semaphore.ToString() -ne $identity.Handle -or $lock.Owner -notmatch '(?i)^C:\\Windows\\(System32|SysWOW64)\\ntdll\.dll$' -or !$handles.ContainsKey($identity.Handle) -or $handles[$identity.Handle].Object -ne $identity.Object -or $handles[$identity.Handle].Type -ne 'Event'){throw 'New Event is not the stable native LoaderLock semaphore'}
  }
  $calls=if($mode -eq 'adapter'){16000}else{8000}
  if($text -notmatch "Parallel=4 Phase=0 Calls=$calls Errors=0 HandleDelta=1" -or $text -notmatch "Parallel=4 Phase=1 Calls=$calls Errors=0 HandleDelta=0"){throw 'Snapshot-mode phase gate changed'}
  $results+=[pscustomobject]@{Architecture=$arch;Mode=$mode;NewHandle=$identity.Handle;ObjectIdentity=$identity.Object;NativeOwner=$loaders.immediate.Owner;LoaderLockOffset=$loaders.immediate.Offset;ColdDelta=1;WarmDelta=0;Measured=$true}
 }
}
if($r.DebuggerFiles.Count -ne 2){throw 'Native image provenance missing'}
[pscustomobject]@{State='Measured';ReceiptSHA256=(Get-FileHash $Receipt).Hash;Results=$results;Scope='New Event in snapshot-mode UTF/control runs is the real PEB LoaderLock semaphore owned by native ntdll and retained through warm phase. Not allocation-call-stack proof or identification of the Event from previous ordinary failed runs; their gates remain Failed.'}|ConvertTo-Json -Depth 5|Set-Content $Output -Encoding UTF8
