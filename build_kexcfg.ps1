$ErrorActionPreference = 'Stop'
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vcRoot\bin\amd64;$vcRoot\bin;$(Split-Path $vcRoot)\Common7\IDE;$sdkRoot\Bin;$env:PATH"
$env:INCLUDE = "$PSScriptRoot\00-Common-Headers;$sdkRoot\Include;$vcRoot\include"
$env:LIB = "$sdkRoot\Lib\x64;$vcRoot\lib\amd64;$PSScriptRoot\00-Import-Libraries"
$out = Join-Path $PSScriptRoot 'x64\Release\KexCfg'
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\main.obj" "$PSScriptRoot\KexCfg\main.c"
if ($LASTEXITCODE) { throw 'KexCfg compilation failed' }
& rc.exe /nologo /i "$PSScriptRoot\KexCfg" "/fo$out\KexCfg.res" "$PSScriptRoot\KexCfg\KexCfg.rc"
if ($LASTEXITCODE) { throw 'KexCfg resource compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:KexCfgEntry /MACHINE:X64 /SUBSYSTEM:WINDOWS,6.0 /LTCG /OPT:REF /OPT:ICF "/OUT:$out\KexCfg.exe" "$out\main.obj" "$out\KexCfg.res" "$PSScriptRoot\x64\Release\KxCfgHlp\KxCfgHlp.lib" "$PSScriptRoot\x64\Release\KexW32ML\KexW32ML.lib" "$PSScriptRoot\x64\Release\KexPathCch\KexPathCch.lib" kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib ktmw32.lib version.lib taskschd.lib msvcrt_x64.lib ntdll_x64.lib
if ($LASTEXITCODE) { throw 'KexCfg link failed' }
