# VxKex_Vista - Unified Build Script
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OUTPUT_DIR = Join-Path $ScriptDir "VistaDLLs"
if (-not (Test-Path $OUTPUT_DIR)) { New-Item -ItemType Directory -Path $OUTPUT_DIR | Out-Null }

function Run-BuildScript([string]$scriptName, [string]$compName) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Building $compName" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    $script = Join-Path $ScriptDir $scriptName
    if (Test-Path $script) {
        & $script
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
            Write-Host "BUILD FAILED: $compName" -ForegroundColor Red
            exit 1
        }
        Write-Host "$compName built successfully" -ForegroundColor Green
    } else {
        Write-Host "WARNING: $scriptName not found, skipping" -ForegroundColor Yellow
    }
}

function Copy-BuiltLib([string]$subDir, [string]$name) {
    $dll = Join-Path $ScriptDir "x64\Release\$subDir\$name.dll"
    $lib = Join-Path $ScriptDir "x64\Release\$subDir\$name.lib"
    if (Test-Path $dll) { Copy-Item -Path $dll -Destination (Join-Path $OUTPUT_DIR "$name.dll") -Force }
    if (Test-Path $lib) { Copy-Item -Path $lib -Destination (Join-Path $OUTPUT_DIR "$name.lib") -Force }
}

# Step 0: Dependency DLLs
Run-BuildScript "build_kexpathcch.ps1" "KexPathCch"
Run-BuildScript "build_kexsmp.ps1" "KexSmp"
Run-BuildScript "build_kexmls.ps1" "KexMls"

$PREBUILT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\x64\Release"
$depDLLs = @("KexPathCch\KexPathCch.lib", "KexSmp\KexSmp.lib", "KexMLS\KexMls.lib")
foreach ($dep in $depDLLs) {
    $srcPath = Join-Path $PREBUILT_LIBS_DIR $dep
    $destPath = Join-Path $OUTPUT_DIR (Split-Path $dep -Leaf)
    if (Test-Path $srcPath) { Copy-Item -Path $srcPath -Destination $destPath -Force }
}

# Build in correct dependency order
Run-BuildScript "build_kexdll.ps1" "KexDll"
Copy-BuiltLib "KexDll" "KexDll"

Run-BuildScript "build_kexw32ml.ps1" "KexW32ML"
Copy-BuiltLib "KexW32ML" "KexW32ML"

Run-BuildScript "build_kxcfghlp.ps1" "KxCfgHlp"
Copy-BuiltLib "KxCfgHlp" "KxCfgHlp"

Run-BuildScript "build_kexgui.ps1" "KexGui"
Copy-BuiltLib "KexGui" "KexGui"

Run-BuildScript "build_kxnt.ps1" "KxNt"
Copy-BuiltLib "KxNt" "KxNt"

Run-BuildScript "build_kxbase.ps1" "KxBase"
Copy-BuiltLib "KxBase" "KxBase"

Run-BuildScript "build_kexshlex.ps1" "KexShlEx"
Copy-BuiltLib "KexShlEx" "KexShlEx"

Run-BuildScript "build_extended.ps1" "Extended DLLs"

Write-Host "`nBUILD COMPLETED SUCCESSFULLY" -ForegroundColor Green


