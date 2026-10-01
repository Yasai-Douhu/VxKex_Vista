param([ValidateSet('x64','x86')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$bin = "$vc\bin"
$sdkLib = "$sdk\Lib"
$vcLib = "$vc\lib"
if ($Architecture -eq 'x64') { $bin += '\amd64'; $sdkLib += '\x64'; $vcLib += '\amd64' }
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$root\00-Common-Headers;$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib;$root\00-Import-Libraries"
$out = "$root\audit\build-vxl-reader\$Architecture"
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$out\vxl_reader_probe.obj" "$PSScriptRoot\vxl_reader_probe.c"
if ($LASTEXITCODE) { throw 'Reader probe compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out\vxl_reader_probe.exe" "$out\vxl_reader_probe.obj" "$out\KexDll.lib" kernel32.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'Reader probe link failed' }
