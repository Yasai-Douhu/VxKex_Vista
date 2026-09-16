param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'x64\Release\KxAdvapi'))
$ErrorActionPreference = 'Stop'
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vcRoot\bin\amd64;$vcRoot\bin;$(Split-Path $vcRoot)\Common7\IDE;$sdkRoot\Bin;$env:PATH"
$env:INCLUDE = "$sdkRoot\Include;$vcRoot\include;$PSScriptRoot\00-Common-Headers"
$env:LIB = "$sdkRoot\Lib\x64;$vcRoot\lib\amd64;$PSScriptRoot\00-Import-Libraries"
$buildOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $buildOutput | Out-Null
$objects = @()
foreach ($source in Get-ChildItem "$PSScriptRoot\KxAdvapi\*.c") {
    $object = Join-Path $buildOutput ($source.BaseName + '.obj')
    & cl.exe /nologo /c /O1 /GL /Gy /Gz /MD /Zi /W3 /TC /GS- /DWIN32 /D_WIN64 /DNDEBUG /DUNICODE /D_UNICODE /DKXADVAPI_EXPORTS "/Fo$object" "/Fd$buildOutput\compile.pdb" $source.FullName
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $object
}
& link.exe /nologo /DLL /LTCG /MACHINE:X64 /ENTRY:DllMain /SUBSYSTEM:WINDOWS,6.0 /OPT:REF /OPT:ICF /DEBUG /RELEASE "/OUT:$buildOutput\KxAdvapi.dll" "/IMPLIB:$buildOutput\KxAdvapi.lib" "/DEF:$PSScriptRoot\KxAdvapi\KxAdvapi.def" @objects KexDll.lib KexPathCch.lib KexSmp.lib KexMls.lib ntdll_x64.lib msvcrt_x64.lib kernel32.lib user32.lib advapi32.lib shlwapi.lib
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
