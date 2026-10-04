param(
 [Parameter(Mandatory=$true)][string]$Archive,
 [Parameter(Mandatory=$true)][string]$Output,
 [string]$FixedReceipt
)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Choose a new output; preserve previous evidence'}
$receiptPath=Join-Path $Archive 'receipt.json'
$r=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
if($r.State -ne 'Measured' -or !$r.ConDrvTrace -or $r.DriverExit -ne 0 -or $r.ZigWriteTraces.Count -ne 2 -or $r.SuiteResults.Count -ne 1){throw 'Require completed two-child console observation'}
if($r.DriverOutput -notmatch 'PASS fresh deployment state restored' -or $r.DriverOutput -notmatch 'PASS owned IFEO key absent after cleanup' -or [regex]::Matches($r.DriverOutput,'PASS owned Zig child IFEO absent after cleanup').Count -ne 2){throw 'Incomplete deployment cleanup'}
$row=$r.SuiteResults[0]
if($row.Architecture -ne 'x64' -or $row.Passed -or $row.Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Require actual x64 failed probe and successful static bindings'}
$handles=@([regex]::Matches($row.Output,'ZigConsoleLaunch Debugger=1 ExpectedHandle=([0-9a-f]+)'))
if($handles.Count -ne 2 -or [regex]::Matches($row.Output,'ZigConsoleWait Wait=00000000 Exit=00000001 Debugger=1').Count -ne 2){throw 'Require both actual children to have failed naturally'}
$results=@()
foreach($trace in $r.ZigWriteTraces){
 $path=Join-Path $Archive "zig-write-$($trace.Index).log"
 $log=[IO.File]::ReadAllText($path)
 if((Get-FileHash $path).Hash -ne $trace.SHA256 -or $log -cne $trace.Output){throw 'Trace file differs from receipt'}
 if($log -match 'Numeric expression missing|Syntax error'){throw 'Debugger command failed'}
 $targetPid=[regex]::Match($log,'(?m)^KXNT_TARGET_PID=([0-9a-f]+)\r?$')
 $entry=[regex]::Match($log,'(?ms)^KXNT_CONSOLE_WRITE_ENTRY\r?\n(.*?)^0:000> bc \*')
 $query=[regex]::Match($log,'(?ms)^KXNT_OBJECT_TYPE_QUERY_ENTRY\r?\n(.*?)^KXNT_OBJECT_TYPE_QUERY_RETURN\r?\n(.*?)^0:000> bc \*')
 $returned=[regex]::Match($log,'(?ms)^KXNT_CONSOLE_WRITE_RETURN\r?\n0:000> r rax\r?\nrax=00000000c00000bb\r?$')
 if(!$targetPid.Success -or !$entry.Success -or !$query.Success -or !$returned.Success -or $entry.Index -ge $query.Index -or $query.Index -ge $returned.Index){throw 'Missing ordered API entry, object query and adapter return'}
 $handle=[regex]::Match($entry.Value,'(?m)^rcx=([0-9a-f]+)\r?$')
 if(!$handle.Success -or [Convert]::ToUInt64($handle.Groups[1].Value,16) -ne [Convert]::ToUInt64($handles[$trace.Index-1].Groups[1].Value,16)){throw 'Actual write handle does not match parent console handle'}
 foreach($register in @('rdx','r8','r9')){if($entry.Value -notmatch "(?m)^$register=0000000000000000\r?`$"){throw 'Event/APC arguments were not NULL'}}
 if($entry.Value -notmatch '00000000`00000028' -or $entry.Value -notmatch '65 20 73 6d 6f 6b 65 0a'){throw 'Missing actual 40-byte Zig payload'}
 if($query.Groups[1].Value -notmatch "(?m)^rcx=$($handle.Groups[1].Value)\r?`$" -or $query.Groups[1].Value -notmatch '(?m)^rdx=0000000000000002\r?$' -or $query.Groups[1].Value -notmatch '(?m)^r9=0000000000000200\r?$' -or $query.Groups[1].Value -notmatch 'KexDll!'){throw 'Wrong query handle, class, buffer length or caller'}
 if($query.Groups[2].Value -notmatch '(?m)^rax=0000000000000000\r?$' -or $query.Groups[2].Value -notmatch '00000000`000a0008' -or $query.Groups[2].Value -notmatch '(?m)^[0-9a-f`]+\s+"File"\r?$'){throw 'Native query did not successfully return TypeName File'}
 if($FixedReceipt){
  $parameters=[regex]::Match($log,'(?m)^KXNT_PROCESS_PARAMETERS=([0-9a-f]+) STDOUT=([0-9a-f]+) CURRENT_DIRECTORY_HANDLE=([0-9a-f]+)\r?$')
  $metadata=[regex]::Match($log,'(?ms)^KXNT_ALIGNED_NATIVE_HANDLE_METADATA\r?\n.*?^Handle ([0-9a-f]+)\r?\n(.*?)^0:000> dq')
  if(!$parameters.Success -or !$metadata.Success -or $parameters.Groups[2].Value -ne $handle.Groups[1].Value){throw 'Missing standard-handle / process-parameters evidence'}
  $aligned=[Convert]::ToUInt64($handle.Groups[1].Value,16) -band ([UInt64]::MaxValue-3)
  if([Convert]::ToUInt64($parameters.Groups[3].Value,16) -ne $aligned -or [Convert]::ToUInt64($metadata.Groups[1].Value,16) -ne $aligned -or $metadata.Groups[2].Value -notmatch 'Type\s+File' -or $metadata.Groups[2].Value -notmatch 'GrantedAccess\s+0x100020:'){throw 'Not the measured non-writable current-directory File alias'}
 }
 $results+=[pscustomobject]@{Index=$trace.Index;PID=$targetPid.Groups[1].Value;Handle=$handle.Groups[1].Value;NativeType='File';QueryStatus='00000000';WriteStatus='c00000bb';TraceSHA256=$trace.SHA256;Entry=$entry.Value;Query=$query.Value;Return=$returned.Value}
}
$fixed=$null
if($FixedReceipt){
 $f=Get-Content $FixedReceipt -Raw|ConvertFrom-Json
 if($f.State -ne 'Passed' -or $f.DriverExit -ne 0 -or !$f.ConDrvSuite -or $f.ConDrvTrace -or $f.SuiteResults.Count -ne 2){throw 'Require uninstrumented ConDrv integration success for both architectures'}
 foreach($suite in $f.SuiteResults){
  $text=$suite.Output
  if(!$suite.Passed -or $text -notmatch 'Failures=0 Result=PASS' -or [regex]::Matches($text,'Case=collision-explicit-error Pass=1').Count -ne 6 -or $text -notmatch 'Case=collision-set-standard-output Pass=1' -or $text -notmatch 'Case=collision-set-standard-error Pass=1' -or $text -notmatch 'Case=collision-console-unchanged Pass=1' -or $text -notmatch 'Case=collision-file-unchanged Pass=1'){throw 'Writable File collisions were not preserved'}
  if([regex]::Matches($text,'ZigConsoleWait Wait=00000000 Exit=00000000 Debugger=0').Count -ne 2 -or [regex]::Matches($text,'ZigConsole Exit=00000000 ReadUnits=39').Count -ne 2 -or [regex]::Matches($text,'ZigRedirect Pipe=[01] Exit=00000000 Bytes=40').Count -ne 2 -or $text -notmatch 'PartialCompletion Injection=owned-KexDll-IAT Calls=6' -or $text -notmatch 'Phase=1 NativeControl=0 HandleDelta=0' -or $suite.Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Missing real stdio, partial completion, warm-resource or binding coverage'}
 }
 if($f.DriverOutput -notmatch 'PASS fresh deployment state restored' -or [regex]::Matches($f.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 2 -or [regex]::Matches($f.DriverOutput,'PASS owned Zig child IFEO absent after cleanup').Count -ne 2){throw 'Fixed integration cleanup incomplete'}
 $fixed=[pscustomobject]@{ReceiptSHA256=(Get-FileHash $FixedReceipt).Hash;Architectures=@($f.SuiteResults.Architecture);UninstrumentedStdioPassed=$true;WritableCollisionCasesPerArchitecture=6}
}
[pscustomobject]@{ReceiptSHA256=(Get-FileHash $receiptPath).Hash;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;Observations=$results;ProbePassed=$false;FixedIntegration=$fixed;Limits='Observed old x64 children under CDB remain FAIL. With FixedReceipt, subsequent uninstrumented stdio and writable-alias rejection are verified separately for both architectures. PEB field layout is NT6 x64 diagnostic-only; no object identity or allocation stack is captured. No full ConDrv emulation, Vista IFEO or global UTF resource completion is claimed.'}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $Output -Encoding UTF8
