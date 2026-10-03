param([Parameter(Mandatory=$true)][string]$InitialArchive,[Parameter(Mandatory=$true)][string]$MeasuredArchive,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Preserve existing evidence; select a new output'}
$rows=@()
foreach($spec in @(@($InitialArchive,'Incomplete','raw-applied-srw-x86.txt',1),@($MeasuredArchive,'Measured','applied-srw-x86.txt',0))){
 $dir=$spec[0];$receiptPath=Join-Path $dir 'receipt.json';$r=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
 if($r.State -ne $spec[1]){throw 'Unexpected receipt state'}
 $image=Join-Path $dir 'native-ntdll-x86.dll'
 if((Get-FileHash -LiteralPath $image).Hash -ne 'A3D767B53F36E97DFEFCAD305AE050C98D5282D0107B7D51A36825C25F544506'){throw 'Private diagnostic RVAs require the measured native image'}
 $logPath=Join-Path $dir 'event-srw-x86.log';$log=Get-Content -LiteralPath $logPath -Raw
 $probePath=Join-Path $dir $spec[2];$probe=Get-Content -LiteralPath $probePath -Raw
 $phases=@([regex]::Matches($log,'(?m)^KXNT_RESOURCE_PHASE\r?\n[0-9a-f]+\s+([0-9a-f]{8})\r?\n([0-9a-f]+)\s+[^\r\n]+\r?\n([0-9a-f]+)\s+([0-9a-f]{8})'))
 if($phases.Count -ne 2 -or $phases[0].Groups[1].Value -ne '00000000' -or $phases[1].Groups[1].Value -ne '00000001'){throw 'Missing ordered resource phases'}
 $slot=$phases[0].Groups[3].Value;$base=$phases[0].Groups[2].Value
 if([Convert]::ToUInt64($slot,16) -ne [Convert]::ToUInt64($base,16)+16){throw 'Unexpected critical-section slot'}
 $stores=@([regex]::Matches($log,'(?m)^KXNT_CS_EVENT_STORE\r?\neax=([0-9a-f]{8})\r?\necx=([0-9a-f]{8})\r?\nedx=([0-9a-f]{8})\r?\n([0-9a-f]+)\s+([0-9a-f]{8})'))
 $owned=@($stores|Where-Object {$_.Groups[3].Value -eq $slot -and $_.Groups[4].Value -eq $slot -and $_.Groups[1].Value -eq '00000000' -and $_.Groups[2].Value -eq $_.Groups[5].Value})
 if($owned.Count -ne 1){throw 'Require exactly one successful loader-lock Event store'}
 $delta=[regex]::Match($probe,'MixedOperations=4000 Threads=4 Writes=2000 HandleDelta=(\d+)')
 if(!$delta.Success -or [int]$delta.Groups[1].Value -ne $spec[3]){throw 'Unexpected actual resource result'}
 $between=$owned[0].Index -gt $phases[0].Index -and $owned[0].Index -lt $phases[1].Index
 if($between -ne ($spec[3] -eq 1)){throw 'Event creation does not explain this measured interval'}
 if($phases[1].Groups[4].Value -ne $owned[0].Groups[2].Value){throw 'Stored Event not retained at phase end'}
 if($spec[3] -eq 1 -and $phases[0].Groups[4].Value -ne '00000000'){throw 'Expected initially empty native loader lock'}
 if($spec[3] -eq 0 -and $phases[0].Groups[4].Value -ne $owned[0].Groups[2].Value){throw 'Expected Event before measurement'}
 $rows+=[pscustomobject]@{ReceiptSHA256=(Get-FileHash $receiptPath).Hash;ReceiptState=$r.State;LogSHA256=(Get-FileHash $logPath).Hash;ProbeSHA256=(Get-FileHash $probePath).Hash;NativeImageSHA256=(Get-FileHash $image).Hash;HandleDelta=[int]$delta.Groups[1].Value;ProbePassed=$probe -match 'Failures=0 Result=PASS';LoaderLockBase=$base;LockSemaphoreSlot=$slot;Before=$phases[0].Groups[4].Value;After=$phases[1].Groups[4].Value;CreatedHandle=$owned[0].Groups[2].Value;CreatedDuringMeasurement=$between;StoreBlock=$owned[0].Value}
}
[pscustomobject]@{AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;Observations=$rows;NativeImageDerivation='Measured x86 ntdll LdrLockLoaderLock pushes image+0xe0060 into RtlEnterCriticalSection; NtCreateEvent return RVA0x397d8 and successful CAS store RVA0x397eb observe LockSemaphore at CS+0x10.';Limits='Diagnostic RVAs apply only to the SHA-guarded native image. The third receipt remains Incomplete because diagnostic PID parsing and debugger-exit classification failed, although raw probe and native Event-store evidence completed. Fourth receipt is Measured, not a replacement of prior failures. Debugger changes timing. These two SRW processes do not attribute historical comparison/UTF Events or prove production resource gates repaired.'}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $Output -Encoding UTF8
