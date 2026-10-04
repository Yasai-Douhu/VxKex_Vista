param(
 [Parameter(Mandatory=$true)][string]$Archive,
 [Parameter(Mandatory=$true)][string]$Output
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
 $results+=[pscustomobject]@{Index=$trace.Index;PID=$targetPid.Groups[1].Value;Handle=$handle.Groups[1].Value;NativeType='File';QueryStatus='00000000';WriteStatus='c00000bb';TraceSHA256=$trace.SHA256;Entry=$entry.Value;Query=$query.Value;Return=$returned.Value}
}
[pscustomobject]@{ReceiptSHA256=(Get-FileHash $receiptPath).Hash;AnalyzerSHA256=(Get-FileHash $PSCommandPath).Hash;Observations=$results;ProbePassed=$false;Limits='Two x64 Zig children under CDB on the disposable Server 2008 clone. Actual adapter handle aliases a native File object; successful type query precedes STATUS_NOT_SUPPORTED. Object identity/name, file ownership and allocation origin are not captured. Debugger output alters console content. No production fix, uninstrumented success, full ConDrv emulation, Vista IFEO or global resource completion is claimed.'}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $Output -Encoding UTF8
