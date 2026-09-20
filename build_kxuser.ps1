param([string]$OutputDirectory)
$repoRoot = $PSScriptRoot
$ErrorActionPreference = 'Stop'
$vcRoot = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdkRoot = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vcRoot\bin\amd64;$vcRoot\bin;$(Split-Path $vcRoot)\Common7\IDE;$sdkRoot\Bin;$env:PATH"
$env:INCLUDE = "$sdkRoot\Include;$vcRoot\include;$repoRoot\00-Common-Headers"
$env:LIB = "$sdkRoot\Lib\x64;$vcRoot\lib\amd64;$repoRoot\00-Import-Libraries"
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repoRoot "x64\Release\KxUser" }; $buildOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $buildOutput | Out-Null
$objects = @()
foreach ($source in Get-ChildItem "$repoRoot\KxUser\*.c") {
    $object = Join-Path $buildOutput ($source.BaseName + '.obj')
    & cl.exe /nologo /c /O1 /GL /Gy /Gz /MD /Zi /W3 /TC /GS- /DWIN32 /D_WIN64 /DNDEBUG /DUNICODE /D_UNICODE /DKXUSER_EXPORTS "/Fo$object" "/Fd$buildOutput\compile.pdb" $source.FullName
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $object
}
& ml64.exe /nologo /c "/Fo$buildOutput\syscal64.obj" "$repoRoot\KxUser\syscal64.asm"
if ($LASTEXITCODE -ne 0) { throw "Assembly failed" }; $objects += "$buildOutput\syscal64.obj"
foreach ($libDirectory in Get-ChildItem "$repoRoot\x64\Release" -Directory) { $env:LIB += ";" + $libDirectory.FullName }
& link.exe /nologo /DLL "/LIBPATH:$repoRoot\x64\Release\KexW32ML" "/LIBPATH:$repoRoot\x64\Release\KexGui" /LTCG /MACHINE:X64 /ENTRY:DllMain /SUBSYSTEM:WINDOWS,6.0 /OPT:REF /OPT:ICF /DEBUG /RELEASE "/OUT:$buildOutput\KxUser.dll" "/IMPLIB:$buildOutput\KxUser.lib" "/DEF:$repoRoot\KxUser\KxUser.def" @objects KexDll.lib KexPathCch.lib KexSmp.lib KexMls.lib ntdll_x64.lib msvcrt_x64.lib kernel32.lib user32.lib advapi32.lib shlwapi.lib gdi32.lib ole32.lib
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
