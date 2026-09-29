$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$out=[IO.Path]::GetFullPath((Join-Path $root '..\..\audit\VistaPty'))
New-Item -ItemType Directory -Force $out | Out-Null
$vc='C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk='C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH="$vc\bin\amd64;$(Split-Path $vc)\Common7\IDE;$env:PATH"
$env:INCLUDE="$sdk\Include;$vc\include"
$env:LIB="$sdk\Lib\x64;$vc\lib\amd64"
& cl.exe /nologo /LD /MT /EHsc /O2 "/I$root\compat" "/Fo$out\addon.obj" "$root\addon.cpp" /link "/OUT:$out\conpty.node" "/IMPLIB:$out\addon.lib" /SUBSYSTEM:WINDOWS,6.0
if($LASTEXITCODE){throw 'Addon build failed'}
& cl.exe /nologo /MT /O2 "/Fo$out\clear.obj" "/Fe$out\vista-pty-clear.exe" "$root\clear.cpp" /link /SUBSYSTEM:CONSOLE,6.0
if($LASTEXITCODE){throw 'Clear helper build failed'}
