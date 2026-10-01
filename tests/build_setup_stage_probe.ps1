param(
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [string]$ConfigurationLibrary = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
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
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$root\00-Common-Headers;$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib;$root\00-Import-Libraries"
$out = "$root\audit\setup_stage_probe_$Architecture"
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$out.obj" "$PSScriptRoot\setup_stage_probe.c"
if ($LASTEXITCODE) { throw 'Probe compilation failed' }
& link.exe /nologo /LTCG /NODEFAULTLIB /ENTRY:mainCRTStartup "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 "/OUT:$out.exe" "$out.obj" $ConfigurationLibrary "$release\KexW32ML\KexW32ML.lib" "$release\KexPathCch\KexPathCch.lib" "$release\KexSmp\KexSmp.lib" "$release\KexMls\KexMls.lib" kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib ktmw32.lib version.lib taskschd.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'Probe link failed' }

