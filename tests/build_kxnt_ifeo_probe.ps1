param([ValidateSet('x86','x64')][string]$Architecture='x64')
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
if($Architecture -eq 'x64'){
 & cl.exe /nologo /MT /O1 /W4 "/Fo$out\ifeo-deployment.obj" "/Fe$out\ifeo-deployment.exe" "$PSScriptRoot\kxnt_ifeo_deployment_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 advapi32.lib shell32.lib
 if($LASTEXITCODE){throw 'Guarded IFEO driver failed'}
}
