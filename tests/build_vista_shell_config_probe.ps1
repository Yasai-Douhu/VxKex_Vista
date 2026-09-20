$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH="$vc\bin\amd64;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE="$sdk\Include;$vc\include;$root\00-Common-Headers"
$env:LIB="$sdk\Lib\x64;$vc\lib\amd64;$root\00-Import-Libraries"
$out=Join-Path $root 'audit\shell-launch\config-test'
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /c /O1 /GS- /Zl /DUNICODE /D_UNICODE /DNDEBUG /D_WIN64 "/Fo$out\probe.obj" "$PSScriptRoot\vista_shell_config_probe.c"
if($LASTEXITCODE){throw 'compile'}
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out\config-probe.exe" "$out\probe.obj" "$root\x64\Release\KxCfgHlp\KxCfgHlp.lib" "$root\x64\Release\KexW32ML\KexW32ML.lib" "$root\x64\Release\KexPathCch\KexPathCch.lib" "$root\x64\Release\KexSmp\KexSmp.lib" "$root\x64\Release\KexMls\KexMls.lib" kernel32.lib user32.lib advapi32.lib version.lib shell32.lib shlwapi.lib ole32.lib oleaut32.lib uuid.lib taskschd.lib msvcrt_x64.lib ntdll_x64.lib
if($LASTEXITCODE){throw 'link'}
