param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'audit\vscode\probe-vs2010' }
$buildOutput = [IO.Path]::GetFullPath($OutputDirectory)
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vcRoot\bin\amd64;$(Split-Path $vcRoot)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$sdkRoot\Include;$vcRoot\include"
$env:LIB = "$sdkRoot\Lib\x64;$vcRoot\lib\amd64"
New-Item -ItemType Directory -Force $buildOutput | Out-Null
& cl.exe /nologo /c /O1 /GS- /Zl "/Fo$buildOutput\probe.obj" "$PSScriptRoot\vista_compat_probe.c"
if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup /MACHINE:X64 /SUBSYSTEM:CONSOLE,6.0 "/OUT:$buildOutput\vista_compat_probe.exe" "$buildOutput\probe.obj" kernel32.lib user32.lib advapi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'Probe link failed' }
