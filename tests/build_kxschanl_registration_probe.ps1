param([ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$bin = "$vc\bin"; $sdkLib = "$sdk\Lib"; $vcLib = "$vc\lib"
if ($Architecture -eq 'x64') { $bin += '\amd64'; $sdkLib += '\x64'; $vcLib += '\amd64' }
$env:PATH = "$bin;$(Split-Path $vc)\Common7\IDE;$sdk\Bin;$env:PATH"
$env:INCLUDE = "$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib"
$out = "$root\audit\kxschanl_registration_probe_$Architecture"
& cl.exe /nologo /O1 /MT "/Fo$out.obj" "/Fe$out.exe" "$PSScriptRoot\kxschanl_registration_probe.c" /link /SUBSYSTEM:CONSOLE,6.0
if ($LASTEXITCODE) { throw 'Registration probe build failed' }
