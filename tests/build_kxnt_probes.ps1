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
