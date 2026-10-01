param(
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [string]$OutputDirectory = '',
    [string]$ConfigurationLibrary = ''
)
$ErrorActionPreference = 'Stop'
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$release = "$PSScriptRoot\Win32\Release"
$bin = "$vc\bin"
$sdkLib = "$sdk\Lib"
$vcLib = "$vc\lib"
if ($Architecture -eq 'x64') {
    $release = "$PSScriptRoot\x64\Release"
    $bin = "$vc\bin\amd64"
    $sdkLib += '\x64'
    $vcLib += '\amd64'
}
if (!$OutputDirectory) { $OutputDirectory = "$release\VxlView" }
if (!$ConfigurationLibrary) { $ConfigurationLibrary = "$release\KxCfgHlp\KxCfgHlp.lib" }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$sdk\Bin;$env:PATH"
$env:INCLUDE = "$PSScriptRoot\00-Common-Headers;$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib;$PSScriptRoot\00-Import-Libraries"
$objects = @()
foreach ($source in Get-ChildItem "$PSScriptRoot\VxlView\*.c") {
    $object = Join-Path $OutputDirectory ($source.BaseName + '.obj')
    & cl.exe /nologo /c /O1 /Gz /GS- /Zl /DUNICODE /D_UNICODE /DNDEBUG "/Fo$object" $source.FullName
    if ($LASTEXITCODE) { throw "VxlView compilation failed: $($source.Name)" }
    $objects += $object
}
& rc.exe /nologo /c65001 /i "$PSScriptRoot\VxlView" "/fo$OutputDirectory\VxlView.res" "$PSScriptRoot\VxlView\VxlView.rc"
if ($LASTEXITCODE) { throw 'VxlView resource compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:EntryPoint "/MACHINE:$Architecture" /SUBSYSTEM:WINDOWS,6.0 /LTCG /OPT:REF /OPT:ICF "/OUT:$OutputDirectory\VxlView.exe" @objects $ConfigurationLibrary "$OutputDirectory\VxlView.res" "$release\KexDll\KexDll.lib" "$release\KexGui\KexGui.lib" "$release\KexW32ML\KexW32ML.lib" "$release\KexPathCch\KexPathCch.lib" "$release\KexSmp\KexSmp.lib" "$release\KexMls\KexMls.lib" comctl32.lib comdlg32.lib uxtheme.lib gdi32.lib kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib version.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'VxlView link failed' }
