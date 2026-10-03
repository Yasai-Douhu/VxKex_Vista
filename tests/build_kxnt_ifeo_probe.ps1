param([ValidateSet('x86','x64')][string]$Architecture='x64',[switch]$Suite,[switch]$UninstrumentedSrw,[switch]$EventPhaseTrace,[switch]$RuntimeSuite)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
$suffix=if($Architecture -eq 'x64'){'\amd64'}else{''}
$sdkSuffix=if($Architecture -eq 'x64'){'\x64'}else{''}
$env:PATH="$vc\bin$suffix;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE="$sdk\Include;$vc\include";$env:LIB="$sdk\Lib$sdkSuffix;$vc\lib$suffix"
$out="$root\audit\KxNtParity\$Architecture"
New-Item -ItemType Directory -Force $out|Out-Null
& lib.exe /nologo "/def:$PSScriptRoot\kxnt_ifeo_imports.def" "/machine:$Architecture" "/out:$out\ifeo-imports.lib"
if($LASTEXITCODE){throw 'Static import library failed'}
& cl.exe /nologo /MT /O1 /W4 /DKXNT_IFEO_IMPORT "/Fo$out\ifeo-open-key.obj" "/Fe$out\KxNtIfeoOpen-$Architecture.exe" "$PSScriptRoot\kxnt_open_key_adapter_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 "$out\ifeo-imports.lib"
if($LASTEXITCODE){throw 'Static import fixture failed'}
$imports=(& "$vc\bin\dumpbin.exe" /imports "$out\KxNtIfeoOpen-$Architecture.exe") -join "`n"
if($LASTEXITCODE -or $imports -notmatch '(?im)^\s+ntdll\.dll\s*$' -or $imports -notmatch 'NtOpenKeyEx' -or $imports -match '(?im)^\s+(KxNt|KexDll)\.dll\s*$'){throw 'Fixture must import native ntdll, never pre-bind the compatibility DLL'}
$imports|Set-Content -Encoding UTF8 "$out\ifeo-imports.txt"
if($Suite -or $RuntimeSuite){
 $suiteOut="$root\audit\KxNtParity\IfeoSuite\$Architecture"
 New-Item -ItemType Directory -Force $suiteOut|Out-Null
 & lib.exe /nologo "/def:$PSScriptRoot\kxnt_ifeo_suite_imports.def" "/machine:$Architecture" "/out:$suiteOut\suite-imports.lib"
 if($LASTEXITCODE){throw 'Suite import library failed'}
 foreach($kind in @('processor-feature','domain','device-family','persisted-state','sid-package','sid-capability','membership','compare','file-information','alert','performance','srw','utf8','silent-exit')){
  $source=if($kind -like 'sid-*'){'sid_class'}else{$kind.Replace('-','_')}
  [string[]]$libs=@(if($kind -eq 'membership'){'advapi32.lib'})
  [string[]]$trace=@(if($kind -eq 'srw' -and !$UninstrumentedSrw){'/DKXNT_IFEO_SRW_RESOURCE_TRACE'})
  if($EventPhaseTrace -and $kind -in @('compare','srw','utf8')){$trace+='/DKXNT_IFEO_RESOURCE_PHASES'}
  $image="$suiteOut\KxNtIfeo-$kind-$Architecture.exe"
  & cl.exe /nologo /MT /O1 /W4 /D_WIN32_WINNT=0x0600 /D_CRT_SECURE_NO_WARNINGS @trace "/FI$PSScriptRoot\kxnt_ifeo_suite_provider.h" "/Fo$suiteOut\$kind.obj" "/Fe$image" "$PSScriptRoot\kxnt_${source}_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 "$suiteOut\suite-imports.lib" @libs
  if($LASTEXITCODE){throw "Detailed IFEO suite build failed: $kind"}
  $table=(& "$vc\bin\dumpbin.exe" /imports $image) -join "`n"
  if($LASTEXITCODE -or $table -notmatch '(?im)^\s+ntdll\.dll\s*$' -or $table -notmatch 'RtlCanonicalizeDomainName' -or $table -notmatch 'NtAlertThreadByThreadId' -or $table -match '(?im)^\s+(KxNt|KexDll)\.dll\s*$'){throw "Wrong static imports: $kind"}
  $table|Set-Content -Encoding UTF8 "$suiteOut\$kind-imports.txt"
 }
}
if($Architecture -eq 'x64'){
 & cl.exe /nologo /MT /O1 /W4 "/Fo$out\ifeo-deployment.obj" "/Fe$out\ifeo-deployment.exe" "$PSScriptRoot\kxnt_ifeo_deployment_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 advapi32.lib shell32.lib
 if($LASTEXITCODE){throw 'Guarded IFEO driver failed'}
}
