param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
if(Test-Path $Output){throw 'Preserve previous status snapshot'}
$checks=@();$evidence=@{}
foreach($item in @(@('core','kxnt-core-ifeo-vista',24),@('condrv','kxnt-condrv-ifeo-vista',2),@('runtime','kxnt-runtime-ifeo-vista',4))){
 $path="$root\docs\validation\$($item[1]).json";$document=Get-Content $path -Raw|ConvertFrom-Json;$r=$document.Receipt
 if($r.State -ne 'Passed' -or $r.DriverExit -ne 0 -or !$r.ClientVista -or $r.EventTrace -or $r.SuiteResults.Count -ne $item[2] -or @($r.SuiteResults|Where-Object {!$_.Passed}).Count){throw "Incomplete ordinary Vista IFEO evidence: $($item[0])"}
 if($r.DriverOutput -notmatch 'OSVersion=6\.0\.6002 ProductType=1 NativeArchitecture=9 DriverKexDllLoaded=0' -or $r.DriverOutput -notmatch 'PASS fresh deployment state restored' -or $r.DriverOutput -match '(?m)^FAIL '){throw 'Native product or teardown not verified'}
 foreach($file in @('KexDll.dll','KxNt.dll','Kex32\KexDll.dll','Kex32\KxNt.dll')){
  $entry=@($r.PackageFiles|Where-Object {$_.Path.Replace('/','\') -eq $file})
  if($entry.Count -ne 1 -or $entry[0].SHA256 -ne (Get-FileHash "$root\Installer\$file").Hash){throw 'Current production candidate differs from tested binaries'}
 }
 foreach($probe in $r.SuiteResults){
  if($probe.Architecture -notin @('x86','x64') -or $probe.Bindings -notmatch 'StaticBindingCount=26 Matches=26 EarlyKexDllLoaded=1 Result=PASS'){throw 'Actual early IFEO binding absent'}
  $checks+=[pscustomobject]@{Architecture=$probe.Architecture;Probe=$probe.Probe;MajorVerification='Passed';Evidence="$($item[1]).json"}
 }
 $evidence[$item[0]]=[pscustomobject]@{SHA256=(Get-FileHash $path).Hash;State=$r.State;DriverExit=$r.DriverExit;Pairs=$r.SuiteResults.Count}
}
$def=[IO.File]::ReadAllText("$root\KxNt\KxNt.def")
$ported=@('NtAlertThreadByThreadId','NtWaitForAlertByThreadId','ZwAlertThreadByThreadId','ZwWaitForAlertByThreadId','ZwCompareObjects','RtlCheckTokenMembershipEx','RtlIsPackageSid','RtlIsCapabilitySid','RtlGetPersistedStateLocation','RtlGetDeviceFamilyInfoEnum','RtlCanonicalizeDomainName','RtlIsProcessorFeaturePresent')
foreach($name in $ported){if($def -notmatch ('(?m)^\s*'+[regex]::Escape($name)+'\s*=\s*KexDll\.')){throw "Missing implemented KexDll export: $name"}}
$importsPath="$root\docs\validation\kxnt-native-import-priority.json";$imports=Get-Content $importsPath -Raw|ConvertFrom-Json
$exportCheck=@'
import importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
root=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('audit',root/'tests/audit_kxnt_application_imports.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
retained='LdrpResGetRCConfig NtCancelDeviceWakeupRequest NtFlushBuffersFileEx NtRequestDeviceWakeup NtRequestWakeupLatency WerCheckEventEscalation WerReportWatsonEvent'.split()
added='NtAlertThreadByThreadId NtWaitForAlertByThreadId ZwAlertThreadByThreadId ZwWaitForAlertByThreadId ZwCompareObjects RtlCheckTokenMembershipEx RtlIsPackageSid RtlIsCapabilitySid RtlGetPersistedStateLocation RtlGetDeviceFamilyInfoEnum RtlCanonicalizeDomainName RtlIsProcessorFeaturePresent'.split()
result={}
for arch,path in [('x86','Installer/Kex32/KxNt.dll'),('x64','Installer/KxNt.dll')]:
    exports=m.PE(root/path).exports()
    if any(n not in exports for n in retained+added): raise RuntimeError('Required package export absent')
    if any(not (exports[n] or '').startswith('KexDll.') for n in added): raise RuntimeError('Added export is not bound to implementation')
    result[arch]={'RetainedVistaOnly':{n:exports[n] for n in retained},'Added':{n:exports[n] for n in added}}
print(json.dumps(result))
'@
$binaryExports=& 'C:\Program Files\PyManager\python.exe' -c $exportCheck $root
if($LASTEXITCODE){throw 'Actual package exports failed validation'}
$binaryExports=$binaryExports|ConvertFrom-Json
$priorities=@()
foreach($arch in @('x86','x64')){
 $native=$imports.Report.NativeForwarders.$arch
 $file=if($arch -eq 'x86'){'Kex32\KxNt.dll'}else{'KxNt.dll'}
 if($native.KxNtSHA256 -ne (Get-FileHash "$root\Installer\$file").Hash){throw 'Import priorities refer to different KxNt binary'}
 $bits=if($arch -eq 'x86'){32}else{64};$apps=@($imports.Report.Applications|Where-Object {$_.Bits -eq $bits})
 $priorities+=[pscustomobject]@{Architecture=$arch;UnresolvedNativeNames=@($native.Unresolved.PSObject.Properties).Count;ApplicationsInspected=$apps.Count;ActualApplicationsWithMissingImports=@($apps|Where-Object {$_.UnresolvedKxNtHits.Count}).Count}
}
[pscustomobject]@{State='MajorVerificationInventory';Policy='User completion criteria updated 2026-10-04: bounded timing/environment differences may be TODO, not blockers, after major semantics/safety/runtime verification';ProductionCandidatesMatchEvidence=$true;NewPublicNamesImplemented=$ported;BinaryExports=$binaryExports;NormalIfeoPairs=$checks;Evidence=$evidence;NativeImportPriority=$priorities;Limits='Inventory of major evidence for current binaries, not proof of arbitrary apps/flags/all resources. WNF remains deferred/unimplemented; AppContainer/full ConDrv/unsupported registry flags remain explicit limits. Prior Failed diagnostic receipts are not changed.'}|ConvertTo-Json -Depth 7|Set-Content $Output -Encoding UTF8
