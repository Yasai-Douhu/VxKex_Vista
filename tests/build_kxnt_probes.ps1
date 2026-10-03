param([ValidateSet('x86','x64')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$bin = "$vc\bin"
$vcLib = "$vc\lib"
$sdkLib = "$sdk\Lib"
if ($Architecture -eq 'x64') {
    $bin += '\amd64'
    $vcLib += '\amd64'
    $sdkLib += '\x64'
}
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib"
$out = "$root\audit\KxNtParity\$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\processor-feature.obj" "/Fe$out\processor-feature.exe" "$PSScriptRoot\kxnt_processor_feature_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "KxNt probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\domain.obj" "/Fe$out\domain.exe" "$PSScriptRoot\kxnt_domain_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Domain probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\device-family.obj" "/Fe$out\device-family.exe" "$PSScriptRoot\kxnt_device_family_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Device family probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\persisted-state.obj" "/Fe$out\persisted-state.exe" "$PSScriptRoot\kxnt_persisted_state_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Persisted state probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\sid-class.obj" "/Fe$out\sid-class.exe" "$PSScriptRoot\kxnt_sid_class_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "SID classification probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\membership.obj" "/Fe$out\membership.exe" "$PSScriptRoot\kxnt_membership_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 advapi32.lib
if ($LASTEXITCODE) { throw "Token membership probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\compare.obj" "/Fe$out\compare.exe" "$PSScriptRoot\kxnt_compare_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Object comparison probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\file-information.obj" "/Fe$out\file-information.exe" "$PSScriptRoot\kxnt_file_information_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "File information probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\alert.obj" "/Fe$out\alert.exe" "$PSScriptRoot\kxnt_alert_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Thread alert probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\console-classification.obj" "/Fe$out\console-classification.exe" "$PSScriptRoot\kxnt_console_classification_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Console classification probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\native-launch.obj" "/Fe$out\native-launch.exe" "$PSScriptRoot\kxnt_native_launch_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Native launch probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\performance.obj" "/Fe$out\performance.exe" "$PSScriptRoot\kxnt_performance_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Performance counter probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\silent-exit.obj" "/Fe$out\silent-exit.exe" "$PSScriptRoot\kxnt_silent_exit_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Silent exit probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\import-resolution.obj" "/Fe$out\import-resolution.exe" "$PSScriptRoot\kxnt_import_resolution_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Import resolution probe build failed ($Architecture)" }
& lib.exe /nologo "/def:$PSScriptRoot\kxnt_condrv_imports.def" "/machine:$Architecture" "/out:$out\condrv-imports.lib"
if ($LASTEXITCODE) { throw "Console profile import library failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\condrv.obj" "/Fe$out\condrv.exe" "$PSScriptRoot\kxnt_condrv_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 "$out\condrv-imports.lib"
if ($LASTEXITCODE) { throw "ConDrv probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\srw.obj" "/Fe$out\srw.exe" "$PSScriptRoot\kxnt_srw_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "SRW lock probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\utf8.obj" "/Fe$out\utf8.exe" "$PSScriptRoot\kxnt_utf8_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "UTF conversion probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\open-key-reference.obj" "/Fe$out\open-key-reference.exe" "$PSScriptRoot\kxnt_open_key_reference_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "Registry open reference probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\registry-transaction.obj" "/Fe$out\registry-transaction.exe" "$PSScriptRoot\kxnt_registry_transaction_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 advapi32.lib KtmW32.lib
if ($LASTEXITCODE) { throw "Registry transaction probe build failed ($Architecture)" }
& cl.exe /nologo /MT /O1 /W4 "/Fo$out\utf8-resource.obj" "/Fe$out\utf8-resource.exe" "$PSScriptRoot\kxnt_utf8_resource_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw "UTF resource probe build failed ($Architecture)" }
