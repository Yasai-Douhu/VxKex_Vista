param(
    [ValidateSet('x64')][string]$Architecture = 'x64',
    [string]$ConfigurationLibrary = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$release = "$root\Win32\Release"
$bin = "$vc\bin"
$sdkLib = "$sdk\Lib"
$vcLib = "$vc\lib"
if ($Architecture -eq 'x64') {
    $release = "$root\x64\Release"
    $bin = "$vc\bin\amd64"
    $sdkLib += '\x64'
    $vcLib += '\amd64'
}
if (!$ConfigurationLibrary) { $ConfigurationLibrary = "$release\KxCfgHlp\KxCfgHlp.lib" }
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$sdk\Bin;$env:PATH"
$env:INCLUDE = "$root\00-Common-Headers;$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib;$root\00-Import-Libraries"
$out = "$root\audit\VistaSetup"
if ($OutputDirectory) { $out = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'VistaSetup' }
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$out.obj" "$root\VistaSetup\main.c"
if ($LASTEXITCODE) { throw 'VistaSetup compilation failed' }
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$out-stage.obj" "$root\VistaSetup\stage.c"
if ($LASTEXITCODE) { throw 'VistaSetup staging compilation failed' }
& rc.exe /nologo /i "$root\VistaSetup" "/fo$out.res" "$root\VistaSetup\VistaSetup.rc"
if ($LASTEXITCODE) { throw 'Setup resources failed' }
& link.exe /nologo /LTCG /NODEFAULTLIB /ENTRY:mainCRTStartup "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out.exe" "$out.obj" "$out-stage.obj" "$out.res" $ConfigurationLibrary "$release\KexW32ML\KexW32ML.lib" "$release\KexPathCch\KexPathCch.lib" "$release\KexSmp\KexSmp.lib" "$release\KexMls\KexMls.lib" kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib ktmw32.lib version.lib taskschd.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'VistaSetup link failed' }

