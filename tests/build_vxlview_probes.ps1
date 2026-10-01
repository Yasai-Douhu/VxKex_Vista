param(
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [string]$OutputDirectory = '',
    [string]$ViewerBuildDirectory = '',
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
    $bin += '\amd64'
    $sdkLib += '\x64'
    $vcLib += '\amd64'
}
if (!$OutputDirectory) { $OutputDirectory = "$root\audit\vxlview-probes\$Architecture" }
if (!$ConfigurationLibrary) { $ConfigurationLibrary = "$release\KxCfgHlp\KxCfgHlp.lib" }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$env:PATH = "$bin;$vc\bin;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE = "$root\00-Common-Headers;$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib;$root\00-Import-Libraries"
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$OutputDirectory\vxl_fixture_probe.obj" "$PSScriptRoot\vxl_fixture_probe.c"
if ($LASTEXITCODE) { throw 'Fixture probe compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:mainCRTStartup "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 "/OUT:$OutputDirectory\vxl_fixture_probe.exe" "$OutputDirectory\vxl_fixture_probe.obj" "$release\KexDll\KexDll.lib" kernel32.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'Fixture probe link failed' }
& cl.exe /nologo /O1 /MT /D_WIN32_WINNT=0x0600 "/Fe$OutputDirectory\vxlview_gui_probe.exe" "/Fo$OutputDirectory\vxlview_gui_probe.obj" "$PSScriptRoot\vxlview_gui_probe.c" /link /SUBSYSTEM:CONSOLE,6.0 user32.lib shell32.lib
if ($LASTEXITCODE) { throw 'Viewer probe build failed' }
if (!$ViewerBuildDirectory) { $ViewerBuildDirectory = "$root\audit\build-vxlview\$Architecture" }
$viewerObjects = @(Get-ChildItem "$ViewerBuildDirectory\*.obj" | ForEach-Object { $_.FullName })
if (!$viewerObjects.Count) { throw 'Build VxlView before building its export probe' }
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$OutputDirectory\vxlview_export_probe.obj" "$PSScriptRoot\vxlview_export_probe.c"
if ($LASTEXITCODE) { throw 'Export probe compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:ExportProbeEntry "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 /LTCG "/OUT:$OutputDirectory\vxlview_export_probe.exe" "$OutputDirectory\vxlview_export_probe.obj" @viewerObjects $ConfigurationLibrary "$release\KexDll\KexDll.lib" "$release\KexGui\KexGui.lib" "$release\KexW32ML\KexW32ML.lib" "$release\KexPathCch\KexPathCch.lib" "$release\KexSmp\KexSmp.lib" "$release\KexMls\KexMls.lib" comctl32.lib comdlg32.lib uxtheme.lib gdi32.lib kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib version.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'Export probe link failed' }
& cl.exe /nologo /c /O1 /Gz /GS- /Zl /DNDEBUG /DUNICODE /D_UNICODE "/Fo$OutputDirectory\vxl_association_probe.obj" "$PSScriptRoot\vxl_association_probe.c"
if ($LASTEXITCODE) { throw 'Association probe compilation failed' }
& link.exe /nologo /NODEFAULTLIB /ENTRY:AssociationProbeEntry "/MACHINE:$Architecture" /SUBSYSTEM:CONSOLE,6.0 /LTCG "/OUT:$OutputDirectory\vxl_association_probe.exe" "$OutputDirectory\vxl_association_probe.obj" @viewerObjects $ConfigurationLibrary "$release\KexDll\KexDll.lib" "$release\KexGui\KexGui.lib" "$release\KexW32ML\KexW32ML.lib" "$release\KexPathCch\KexPathCch.lib" "$release\KexSmp\KexSmp.lib" "$release\KexMls\KexMls.lib" comctl32.lib comdlg32.lib uxtheme.lib gdi32.lib kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib version.lib "msvcrt_$Architecture.lib" "ntdll_$Architecture.lib"
if ($LASTEXITCODE) { throw 'Association probe link failed' }
