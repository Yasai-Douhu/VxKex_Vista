$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$SDK71_BIN = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin"
$SDK71_INCLUDE = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include"
$SDK71_LIB = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib"

$env:PATH = "$VS10_BIN;$SDK71_BIN;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$SRC_DIR = Join-Path $ScriptDirAbs "KexSmp"
$OUT_DIR = Join-Path $ScriptDirAbs "Win32\Release\KexSmp"

if (-not (Test-Path $OUT_DIR)) { New-Item -ItemType Directory -Path $OUT_DIR | Out-Null }

$src = Join-Path $SRC_DIR "strmap.c"
$objFile = Join-Path $OUT_DIR "strmap.obj"

$defines = @("/D", "WIN32", "/D", "NDEBUG", "/D", "_LIB", "/D", "UNICODE", "/D", "_UNICODE")
$includeFlags = @("/I", "`"$HDR_DIR`"")
$flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-") + $defines + $includeFlags + @("/Fo$objFile", "`"$src`"")

& "cl.exe" @flags
if ($LASTEXITCODE -ne 0) { exit 1 }

$libPath = Join-Path $OUT_DIR "KexSmp.lib"
$libArgs = @("/NOLOGO", "/LTCG", "/OUT:`"$libPath`"", "`"$objFile`"")

& "lib.exe" @libArgs
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "KexSmp.lib (x86) built successfully!" -ForegroundColor Green
