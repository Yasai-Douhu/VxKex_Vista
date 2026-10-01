param(
    [string]$ConfigurationLibrary = '',
    [string]$OutputDirectory = '',
    [ValidateSet('x64','x86')][string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$compilerDir = "$vcRoot\bin"
$releaseDir = "$PSScriptRoot\Win32\Release"
$sdkLib = "$sdkRoot\Lib"
$vcLib = "$vcRoot\lib"
$machine = 'X86'
if ($Architecture -eq 'x64') {
    $compilerDir = "$vcRoot\bin\amd64"
    $releaseDir = "$PSScriptRoot\x64\Release"
    $sdkLib = "$sdkRoot\Lib\x64"
    $vcLib = "$vcRoot\lib\amd64"
    $machine = 'X64'
}
$env:PATH = "$compilerDir;$vcRoot\bin;$(Split-Path $vcRoot)\Common7\IDE;$sdkRoot\Bin;$env:PATH"
$env:INCLUDE = "$PSScriptRoot\00-Common-Headers;$sdkRoot\Include;$vcRoot\include"
$env:LIB = "$sdkLib;$vcLib;$PSScriptRoot\00-Import-Libraries"
$out = Join-Path $releaseDir 'KexCfg'
if ($OutputDirectory) { $out = $OutputDirectory }
if (!$ConfigurationLibrary) { $ConfigurationLibrary = "$releaseDir\KxCfgHlp\KxCfgHlp.lib" }
New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\main.obj" "$PSScriptRoot\KexCfg\main.c"
if ($LASTEXITCODE) { throw 'KexCfg compilation failed' }
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\gui.obj" "$PSScriptRoot\KexCfg\gui.c"
if ($LASTEXITCODE) { throw 'KexCfg GUI compilation failed' }
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\global.obj" "$PSScriptRoot\KexCfg\global.c"
if ($LASTEXITCODE) { throw 'KexCfg global settings compilation failed' }
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\bulk.obj" "$PSScriptRoot\KexCfg\bulk.c"
if ($LASTEXITCODE) { throw 'KexCfg bulk settings compilation failed' }
& cl.exe /nologo /c /O1 /GS- /Zl /Gz /DUNICODE /D_UNICODE /DNDEBUG "/Fo$out\writer.obj" "$PSScriptRoot\KexCfg\writer.c"
if ($LASTEXITCODE) { throw 'KexCfg writer client compilation failed' }
& rc.exe /nologo /i "$PSScriptRoot\KexCfg" "/fo$out\KexCfg.res" "$PSScriptRoot\KexCfg\KexCfg.rc"
if ($LASTEXITCODE) { throw 'KexCfg resource compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:KexCfgEntry /MACHINE:$machine /SUBSYSTEM:WINDOWS,6.0 /LTCG /OPT:REF /OPT:ICF "/OUT:$out\KexCfg.exe" "$out\main.obj" "$out\gui.obj" "$out\global.obj" "$out\bulk.obj" "$out\writer.obj" "$out\KexCfg.res" $ConfigurationLibrary "$releaseDir\KexW32ML\KexW32ML.lib" "$releaseDir\KexPathCch\KexPathCch.lib" "$releaseDir\KexGui\KexGui.lib" "$releaseDir\KexSmp\KexSmp.lib" "$releaseDir\KexMls\KexMls.lib" comctl32.lib comdlg32.lib uxtheme.lib gdi32.lib kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib ktmw32.lib version.lib taskschd.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'KexCfg link failed' }
