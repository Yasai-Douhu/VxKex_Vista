[CmdletBinding()]
param([string[]]$Only = @(), [string]$OutputRoot = '')

# Build the x86 compatibility libraries shipped with the x64-host installer.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (!$OutputRoot) { $OutputRoot = Join-Path $root 'Win32\Release' }
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC'
$sdk = 'C:\Program Files\Microsoft SDKs\Windows\v7.1'
$env:PATH = "$vc\bin;$(Split-Path $vc)\Common7\IDE;$sdk\Bin;$env:PATH"
$env:INCLUDE = "$sdk\Include;$vc\include;$root\00-Common-Headers"
$env:LIB = "$OutputRoot\KxCryp;$sdk\Lib;$vc\lib;$root\00-Import-Libraries"
foreach ($dir in Get-ChildItem "$root\Win32\Release" -Directory) { $env:LIB += ';' + $dir.FullName }
$names = @('KxAdvapi','KxCom','KxCrt','KxCryp','KxDw','KxDx','KxMi','KxNet','KxSChanl','KxUia','KxUser')
if ($Only.Count -gt 0) {
 foreach ($requested in $Only) {
  if ($requested -notin $names) { throw "Unknown extended DLL: $requested" }
 }
 $names = @($names | Where-Object { $_ -in $Only })
}
foreach ($name in $names) {
 $out = Join-Path $OutputRoot $name
 New-Item -ItemType Directory -Force $out | Out-Null
 $env:LIB += ';' + $out
 $objects = @()
 foreach ($source in Get-ChildItem "$root\$name\*.c") {
  $obj = Join-Path $out ($source.BaseName + '.obj')
  $extra = @()
  if ($name -eq 'KxSChanl') { $extra = @('/Oi') }
  & cl.exe /nologo /c /O1 /GL /Gy /Gz /MD /Zi /W3 /TC /GS- /DWIN32 /DNDEBUG /DUNICODE /D_UNICODE @extra "/D$($name.ToUpper())_EXPORTS" "/Fo$obj" "/Fd$out\compile.pdb" $source.FullName
  if ($LASTEXITCODE) { throw "Compilation failed: $source" }
  $objects += $obj
 }
 $resourceSource = Join-Path $root "$name\$name.rc"
 if (Test-Path -LiteralPath $resourceSource) {
  $resource = Join-Path $out "$name.res"
  & rc.exe /nologo /i "$root\00-Common-Headers" /fo $resource $resourceSource
  if ($LASTEXITCODE) { throw "Resource compilation failed: $name" }
  $objects += $resource
 }
 # syscal64.asm belongs only to x64; x86 uses its C/inline-assembly path.
 $def = Get-ChildItem "$root\$name\*.def" | Select-Object -First 1
 & link.exe /nologo /DLL /LTCG /MACHINE:X86 /ENTRY:DllMain /SUBSYSTEM:WINDOWS,6.0 /OPT:REF /OPT:ICF "/OUT:$out\$name.dll" "/IMPLIB:$out\$name.lib" "/DEF:$($def.FullName)" @objects "$root\Win32\Release\KexDll\KexDll.lib" "$root\Win32\Release\KexPathCch\KexPathCch.lib" "$root\Win32\Release\KexSmp\KexSmp.lib" "$root\Win32\Release\KexMls\KexMls.lib" "$root\Win32\Release\KexW32ML\KexW32ML.lib" "$root\Win32\Release\KexGui\KexGui.lib" ntdll_x86.lib msvcrt_x86.lib kernel32.lib user32.lib advapi32.lib shlwapi.lib gdi32.lib ole32.lib version.lib
 if ($LASTEXITCODE) { throw "Link failed: $name" }
}
