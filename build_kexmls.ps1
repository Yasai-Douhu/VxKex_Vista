$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64"
$SDK71_BIN = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin"
$SDK71_INCLUDE = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Include"
$SDK71_LIB = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64"

$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$SRC_DIR = Join-Path $ScriptDirAbs "KexMLS"
$OUT_DIR = Join-Path $ScriptDirAbs "x64\Release\KexMLS"

if (-not (Test-Path $OUT_DIR)) { New-Item -ItemType Directory -Path $OUT_DIR | Out-Null }

$src = Join-Path $SRC_DIR "mls.c"
$objFile = Join-Path $OUT_DIR "mls.obj"

$defines = @("/D", "WIN32", "/D", "NDEBUG", "/D", "_LIB", "/D", "UNICODE", "/D", "_UNICODE")
$includeFlags = @("/I", "`"$HDR_DIR`"")
$flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-") + $defines + $includeFlags + @("/Fo$objFile", "`"$src`"")

& "cl.exe" @flags
if ($LASTEXITCODE -ne 0) { exit 1 }

$libPath = Join-Path $OUT_DIR "KexMLS.lib"
$libArgs = @("/NOLOGO", "/LTCG", "/OUT:`"$libPath`"", "`"$objFile`"")

& "lib.exe" @libArgs
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "KexMLS.lib (x64) built successfully!" -ForegroundColor Green

