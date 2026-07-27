# VxKex_Vista KexW32ML Build Script with VS2010 (cl.exe)
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VxKex_Vista KexW32ML Build with VS2010 (cl.exe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64"
$SDK71_BIN = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin"
$SDK71_INCLUDE = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include"
$SDK71_LIB = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib\x64"

$env:PATH = "$VS10_BIN;$SDK71_BIN;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$SRC_DIR = Join-Path $ScriptDirAbs "KexW32ML"
$OUT_DIR = Join-Path $ScriptDirAbs "x64\Release\KexW32ML"

if (-not (Test-Path $OUT_DIR)) { New-Item -ItemType Directory -Path $OUT_DIR | Out-Null }

function Invoke-ClCompile {
    param(
        [string]$SourceFile,
        [string]$OutputFile,
        [string[]]$Defines,
        [string]$OutputDir
    )
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
    
    $includeFlags = @("/I", "`"$HDR_DIR`"")
    $foFlag = "/Fo" + $OutputFile
    $srcFlag = "`"$SourceFile`""
    $flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-") + $Defines + $includeFlags + @($foFlag, $srcFlag)
    $srcName = Split-Path $SourceFile -Leaf
    Write-Host "  Compiling: " $srcName -NoNewline
    & "cl.exe" @flags
    if ($LASTEXITCODE -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        return $false
    }
    Write-Host " OK" -ForegroundColor Green
    return $true
}

Write-Host "" -ForegroundColor Yellow
Write-Host "[1/2] Compiling KexW32ML source files..." -ForegroundColor Yellow

$Sources = @("file.c", "kexw32ml.c", "registry.c", "txapi.c")
$ObjFiles = @()
$allOk = $true

foreach ($srcFile in $Sources) {
    $src = Join-Path $SRC_DIR $srcFile
    $objName = $srcFile -replace '\.c$', '.obj'
    $objFile = Join-Path $OUT_DIR $objName
    $ObjFiles += $objFile
    
    $defines = @("/D", "WIN32", "/D", "_WIN64", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "KW32ML_EXPORTS", "/D", "UNICODE", "/D", "_UNICODE")
    
    if (!(Invoke-ClCompile $src $objFile $defines $OUT_DIR)) {
        $allOk = $false
        break
    }
}

if (-not $allOk) {
    Write-Host "" -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}

$libPath = Join-Path $OUT_DIR "KexW32ML.lib"

Write-Host "" -ForegroundColor Yellow
Write-Host "[2/2] Archiving KexW32ML.lib..." -ForegroundColor Yellow

$libArgs = @("/NOLOGO", "/LTCG", "/MACHINE:X64", "/OUT:`"$libPath`"")
foreach ($obj in $ObjFiles) {
    $libArgs += "`"$obj`""
}

& "lib.exe" @libArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}

Write-Host " OK" -ForegroundColor Green

if (Test-Path $libPath) {
    $libItem = Get-Item $libPath
    $libSizeKB = [math]::Round($libItem.Length / 1KB, 2)
    Write-Host "" -ForegroundColor Green
    Write-Host "  KexW32ML.lib: OK ($libSizeKB KB)" -ForegroundColor Green
}

exit 0
