param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
if (!$OutputDirectory) { $OutputDirectory = Join-Path $PSScriptRoot '..\..\audit\VistaRun' }
$buildOutput = [IO.Path]::GetFullPath($OutputDirectory)
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vcRoot\bin\amd64;$(Split-Path $vcRoot)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$sdkRoot\Include;$vcRoot\include"
$env:LIB = "$sdkRoot\Lib\x64;$vcRoot\lib\amd64"
New-Item -ItemType Directory -Force $buildOutput | Out-Null
& cl.exe /nologo /c /O1 /GS- /Gs65536 /Zl "/Fo$buildOutput\main.obj" "$PSScriptRoot\main.c"
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup /MACHINE:X64 /SUBSYSTEM:WINDOWS,6.0 "/OUT:$buildOutput\VistaRun.exe" "$buildOutput\main.obj" kernel32.lib user32.lib advapi32.lib version.lib
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
