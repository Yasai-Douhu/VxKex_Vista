param([Parameter(Mandatory=$true)][string]$Archive,[Parameter(Mandatory=$true)][string]$NativeReference,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Preserve earlier analysis; select a new output'}
$receiptPath=Join-Path $Archive 'receipt.json';$r=Get-Content $receiptPath -Raw|ConvertFrom-Json
if($r.State -ne 'Measured' -or !$r.Utf8Trace -or $r.DriverExit -ne 0 -or $r.EventResults.Count -ne 1 -or !$r.EventResults[0].Measured){throw 'Require completed UTF Event observation and teardown'}
if($r.DriverOutput -notmatch 'PASS fresh deployment state restored' -or [regex]::Matches($r.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 1){throw 'Missing owned profile cleanup'}
$image=Join-Path $Archive 'native-ntdll-x86.dll'
if((Get-FileHash $image).Hash -ne 'A3D767B53F36E97DFEFCAD305AE050C98D5282D0107B7D51A36825C25F544506'){throw 'Internal diagnostic RVAs require the exact measured native image'}
$row=$r.EventResults[0];$log=$row.Trace
$phases=@([regex]::Matches($log,'(?m)^KXNT_RESOURCE_PHASE\r?\n[0-9a-f]+\s+([0-9a-f]{8})\r?\n([0-9a-f]+)\s+[^\r\n]+\r?\n([0-9a-f]+)\s+([0-9a-f]{8})'))
if($phases.Count -ne 4){throw 'Missing four ordered measurement boundaries'}
$slot=$phases[0].Groups[3].Value;$base=$phases[0].Groups[2].Value
if([Convert]::ToUInt64($slot,16) -ne [Convert]::ToUInt64($base,16)+16){throw 'Unexpected LockSemaphore slot'}
for($i=0;$i -lt 4;$i++){if([Convert]::ToUInt32($phases[$i].Groups[1].Value,16) -ne $i -or $phases[$i].Groups[3].Value -ne $slot){throw 'Inconsistent process or phase order'}}
$stores=@([regex]::Matches($log,'(?m)^KXNT_CS_EVENT_STORE\r?\neax=([0-9a-f]{8})\r?\necx=([0-9a-f]{8})\r?\nedx=([0-9a-f]{8})\r?\n([0-9a-f]+)\s+([0-9a-f]{8})'))
$owned=@($stores|Where-Object {$_.Groups[1].Value -eq '00000000' -and $_.Groups[3].Value -eq $slot -and $_.Groups[4].Value -eq $slot -and $_.Groups[2].Value -eq $_.Groups[5].Value})
if($owned.Count -ne 1 -or $owned[0].Index -lt $phases[0].Index -or $owned[0].Index -gt $phases[1].Index){throw 'Missing same-process successful native loader-lock store in the first interval'}
$handle=$owned[0].Groups[2].Value
if($phases[0].Groups[4].Value -ne '00000000'){throw 'Loader lock Event existed before the measured first interval'}
foreach($m in $phases[1..3]){if($m.Groups[4].Value -ne $handle){throw 'Event not retained through both intervals'}}
$interval=$log.Substring($phases[0].Index,$owned[0].Index-$phases[0].Index)
$creation=[regex]::Match($interval,'(?ms)^KXNT_EVENT_CREATE32\r?\n.*?(?=^KXNT_EVENT_CREATE_NATIVE)')
$returned=[regex]::Match($interval,'(?m)^KXNT_CS_EVENT_RETURN\r?\neax=00000000\r?\n[0-9a-f]+\s+([0-9a-f]{8})\r?\n[0-9a-f]+\s+([0-9a-f]{8})')
if(!$creation.Success -or $creation.Value -notmatch 'LdrInitializeThunk' -or !$returned.Success -or $returned.Groups[1].Value -ne $handle -or $returned.Groups[2].Value -ne $base){throw 'Missing Event creation stack or successful matching return'}
$parallel=@([regex]::Matches($row.Output,'Parallel=4 Phase=(\d) Calls=16000 Errors=0 HandleDelta=(-?\d+)'))
if($parallel.Count -ne 2 -or $parallel[0].Groups[2].Value -ne '1' -or $parallel[1].Groups[2].Value -ne '0'){throw 'Unexpected actual parallel resource measurements'}
$native=Get-Content $NativeReference -Raw
$actual=@($row.Output -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
$expected=@($native -split '\r?\n'|Where-Object {$_ -match '^(Call=|Pointer=|Scalars=|Overlap=)'})
if($actual.Count -ne 4893 -or ($actual -join "`n") -cne ($expected -join "`n")){throw 'UTF detailed values differ from saved native reference'}
[pscustomobject]@{ReceiptSHA256=(Get-FileHash $receiptPath).Hash;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;NativeImageSHA256=(Get-FileHash $image).Hash;NativeReferenceSHA256=(Get-FileHash $NativeReference).Hash;NativeValueRowsEqual=$true;Rows=$actual.Count;LoaderLockBase=$base;LockSemaphoreSlot=$slot;CreatedHandle=$handle;PhaseSlots=@($phases|ForEach-Object {$_.Groups[4].Value});CreatedDuringColdInterval=$true;ColdHandleDelta=1;WarmHandleDelta=0;CreationStack=$creation.Value;ReturnBlock=$returned.Value;StoreBlock=$owned[0].Value;ProbePassed=$row.ProbePassed;Limits='One WOW64 process under CDB. Native loader-lock identity derives from LdrLockLoaderLock disassembly for the SHA-guarded image, not nearest-export stack labels. The event is retained through phase3; full process-exit cleanup is not traced. Debugger shifts contention: previous nondebug x86/x64 warm delta1 failures remain unexplained for those historical processes. No global UTF resource completion or x64 Event attribution.'}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $Output -Encoding UTF8
