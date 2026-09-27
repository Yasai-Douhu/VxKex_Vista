$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$sdk\Include;$vc\include"
$env:LIB = "$sdk\Lib;$vc\lib"
$out = Join-Path $root 'audit\wow64'
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /c /O1 /GS- /Zl "/Fo$out\version-probe.obj" "$PSScriptRoot\wow64_version_probe.c"
if ($LASTEXITCODE) { throw 'Compile failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup /MACHINE:X86 /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out\VxKexWow64Probe.exe" "$out\version-probe.obj" kernel32.lib user32.lib
if ($LASTEXITCODE) { throw 'Link failed' }
Copy-Item "$out\VxKexWow64Probe.exe" "$out\VxKexChildNoIfeo.exe" -Force
& cl.exe /nologo /c /O1 /GS- /Zl "/Fo$out\child-probe.obj" "$PSScriptRoot\wow64_child_probe.c"
if ($LASTEXITCODE) { throw 'Child launcher compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup /MACHINE:X86 /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out\VxKexWow64Parent.exe" "$out\child-probe.obj" kernel32.lib user32.lib
if ($LASTEXITCODE) { throw 'Child launcher link failed' }
