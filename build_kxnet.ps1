param(
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$platform = if ($Architecture -eq 'x64') { 'x64' } else { 'Win32' }
$bin = if ($Architecture -eq 'x64') { "$vc\bin\amd64" } else { "$vc\bin" }
$sdkLib = if ($Architecture -eq 'x64') { "$sdk\Lib\x64" } else { "$sdk\Lib" }
$vcLib = if ($Architecture -eq 'x64') { "$vc\lib\amd64" } else { "$vc\lib" }
if (!$OutputDirectory) { $OutputDirectory = "$repo\$platform\Release\KxNet" }
$out = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH = "$bin;$vc\..\Common7\IDE;$env:PATH"
$env:INCLUDE = "$sdk\Include;$vc\include"
$env:LIB = "$sdkLib;$vcLib"
$imports = "$repo\00-Import-Libraries"
$objects = @()
foreach ($source in Get-ChildItem "$repo\KxNet" -Filter '*.c') {
    $obj = Join-Path $out ($source.BaseName + '.obj')
    $objects += $obj
    $flags = @('/nologo','/c','/O1','/Os','/GL','/Gy','/Gz','/MD','/Z7','/W3','/TC','/GS-',
        '/DWIN32','/DNDEBUG','/D_WINDOWS','/D_USRDLL','/DKxNet_EXPORTS',
        '/D_WIN32_WINNT=0x0600','/DWINVER=0x0600',"/I$repo\00-Common-Headers",
        "/Fo$obj",$source.FullName)
    & "$bin\cl.exe" @flags
    if ($LASTEXITCODE) { throw "Compile failed: $($source.Name)" }
}
$libraries = @("$imports\ntdll_$Architecture.lib", "$imports\msvcrt_$Architecture.lib")
foreach ($name in @('KexDll','KexPathCch','KexSmp','KexMls')) {
    $lib = if ($Architecture -eq 'x64') { "$imports\$name.lib" } else { "$repo\Win32\Release\$name\$name.lib" }
    if (!(Test-Path -LiteralPath $lib)) { throw "Missing dependency: $lib" }
    $libraries += $lib
}
$flags = @('/NOLOGO','/DLL','/INCREMENTAL:NO','/SUBSYSTEM:WINDOWS,6.00',
    '/ENTRY:DllMain','/OPT:REF','/OPT:ICF','/LTCG','/RELEASE',"/MACHINE:$Architecture",
    "/OUT:$out\KxNet.dll","/IMPLIB:$out\KxNet.lib","/DEF:$repo\KxNet\kxnet.def",
    "/LIBPATH:$imports") + $objects + $libraries + @('kernel32.lib','user32.lib','gdi32.lib','advapi32.lib','shlwapi.lib')
if ($Architecture -eq 'x86') {
    foreach ($dir in Get-ChildItem "$repo\Win32\Release" -Directory) {
        $flags += "/LIBPATH:$($dir.FullName)"
    }
}
& "$bin\link.exe" @flags
if ($LASTEXITCODE) { throw 'KxNet link failed' }
Write-Output "Built $out\KxNet.dll ($Architecture)"
