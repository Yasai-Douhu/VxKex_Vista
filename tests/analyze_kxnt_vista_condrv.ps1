param([Parameter(Mandatory=$true)][string]$Receipt,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path $Output){throw 'Preserve prior analysis'}
$r=Get-Content $Receipt -Raw|ConvertFrom-Json
if(!$r.ClientVista -or !$r.ConDrvSuite -or $r.ConDrvTrace -or $r.EventTrace -or $r.State -ne 'Passed' -or $r.DriverExit -ne 0){throw 'Require normal Vista ConDrv integration success'}
if($r.DriverOutput -notmatch 'OSVersion=6\.0\.[0-9]+ ProductType=1 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -match '(?m)^FAIL ' -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored'){throw 'Invalid native product or failed lifecycle'}
if($r.SuiteResults.Count -ne 2){throw 'Require both architecture results'}
$results=@()
foreach($arch in @('x86','x64')){
 $items=@($r.SuiteResults|Where-Object {$_.Architecture -eq $arch -and $_.Probe -eq 'condrv'})
 if($items.Count -ne 1 -or !$items[0].Passed){throw 'Missing architecture'}
 $text=$items[0].Output;$control=$items[0].LookupControlOutput
 if($items[0].Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS' -or [regex]::Matches($items[0].Bindings,'(?m)^Import=.*Equal=1 Owner=.+').Count -ne 26){throw 'Actual static import binding incomplete'}
 foreach($log in @($text,$control)){if($log -notmatch 'Failures=0 Result=PASS' -or $log -match 'Pass=0'){throw 'Behavior failed'}}
 if([regex]::Matches($text,'ZigConsole Exit=00000000 ReadUnits=39').Count -ne 2 -or [regex]::Matches($text,'ZigConsoleWait Wait=00000000 Exit=00000000 Debugger=0').Count -ne 2 -or [regex]::Matches($text,'ZigRedirect Pipe=[01] Exit=00000000 Bytes=40').Count -ne 2){throw 'Actual Zig stdio incomplete'}
 if([regex]::Matches($text,'Case=collision-explicit-error Pass=1').Count -ne 6 -or $text -notmatch 'Case=collision-console-unchanged Pass=1' -or $text -notmatch 'Case=collision-file-unchanged Pass=1' -or $text -notmatch 'PartialCompletion Injection=owned-KexDll-IAT Calls=6' -or $text -notmatch 'Case=vt-output Pass=1'){throw 'Collision, partial completion or VT gate missing'}
 $cold=[regex]::Match($text,'ConcurrentWrites=4000 Threads=4 Phase=0 NativeControl=0 HandleDelta=(-?\d+)')
 $native=[regex]::Match($control,'ConcurrentWrites=4000 Threads=4 Phase=0 NativeControl=1 HandleDelta=(-?\d+)')
 if(!$cold.Success -or !$native.Success -or $cold.Groups[1].Value -ne $native.Groups[1].Value -or $text -notmatch 'Phase=1 NativeControl=0 HandleDelta=0' -or $control -notmatch 'Phase=1 NativeControl=1 HandleDelta=0' -or $text -notmatch 'Case=concurrent-handle-identities Pass=1' -or $control -notmatch 'Case=concurrent-handle-identities Pass=1'){throw 'Native-controlled resources incomplete'}
 $results+=[pscustomobject]@{Architecture=$arch;Passed=$true;ConsoleRuns=2;ConsoleUnits=39;RedirectedBytes=40;AmbiguousFileRejections=6;PartialCalls=6;ColdHandleDelta=[int]$cold.Groups[1].Value;NativeColdHandleDelta=[int]$native.Groups[1].Value;WarmHandleDelta=0;FixtureSHA256=$items[0].FixtureSHA256}
}
if([regex]::Matches($r.DriverOutput,'PASS installed KexDll/KxNt byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS installed package-root DLL byte-identical').Count -ne 4 -or [regex]::Matches($r.DriverOutput,'PASS unregistered real Zig child fails before main').Count -ne 2 -or [regex]::Matches($r.DriverOutput,'PASS owned IFEO key absent after cleanup').Count -ne 2 -or [regex]::Matches($r.DriverOutput,'PASS owned Zig child IFEO absent after cleanup').Count -ne 2){throw 'Install bytes, child negative control or cleanup missing'}
[pscustomobject]@{State='Passed';ReceiptSHA256=(Get-FileHash $Receipt).Hash;Results=$results;Scope='Vista client normal IFEO, limited Zig-content-profile legacy console writes, real Zig console/file/pipe stdio, ambiguous File refusal and native-controlled resources. No full ConDrv namespace, CDB object-allocation trace, native x86 OS or global UTF gate completion.'}|ConvertTo-Json -Depth 5|Set-Content $Output -Encoding UTF8
