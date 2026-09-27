# Assemble only architecture-checked runtime binaries, never shell/helper DLLs.
$ErrorActionPreference = 'Stop'
$out = Join-Path $PSScriptRoot 'Installer\Kex32'
New-Item -ItemType Directory -Force $out | Out-Null
foreach ($name in @('KexDll','KxBase','KxNt','KxAdvapi','KxCom','KxCrt','KxCryp','KxDw','KxDx','KxMi','KxNet','KxUia','KxUser')) {
 $source = Join-Path $PSScriptRoot "Win32\Release\$name\$name.dll"
 $bytes = [IO.File]::ReadAllBytes($source)
 $pe = [BitConverter]::ToInt32($bytes, 0x3c)
 if ([BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c) { throw "Not an x86 PE: $source" }
 Copy-Item -LiteralPath $source -Destination (Join-Path $out "$name.dll") -Force
}
