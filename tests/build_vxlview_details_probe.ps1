param([ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
$bin="$vc\bin"; $sdkLib="$sdk\Lib"; $vcLib="$vc\lib"
if($Architecture -eq 'x64') { $bin+='\amd64'; $sdkLib+='\x64'; $vcLib+='\amd64' }
$env:PATH="$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE="$vc\include;$sdk\Include"; $env:LIB="$vcLib;$sdkLib"
$out="$root\audit\build-vxl-details\$Architecture"
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /O1 /MT /D_WIN32_WINNT=0x0600 "/Fe$out\vxlview_details_probe.exe" "/Fo$out\vxlview_details_probe.obj" "$PSScriptRoot\vxlview_details_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 user32.lib shell32.lib
if($LASTEXITCODE) { throw 'Viewer details probe build failed' }
