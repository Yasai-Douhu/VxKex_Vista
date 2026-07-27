$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OUTPUT_DIR = Join-Path $ScriptDir "Win32DLLs"
if (-not (Test-Path $OUTPUT_DIR)) { New-Item -ItemType Directory -Path $OUTPUT_DIR | Out-Null }

function Run-BuildScript([string]$scriptName, [string]$compName) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Building $compName (x86)" -ForegroundColor Cyan
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
    $dll = Join-Path $ScriptDir "Win32\Release\$subDir\$name.dll"
    $lib = Join-Path $ScriptDir "Win32\Release\$subDir\$name.lib"
    if (Test-Path $dll) { Copy-Item -Path $dll -Destination (Join-Path $OUTPUT_DIR "$name.dll") -Force }
    if (Test-Path $lib) { Copy-Item -Path $lib -Destination (Join-Path $OUTPUT_DIR "$name.lib") -Force }
}

Run-BuildScript "build_kexpathcch_x86.ps1" "KexPathCch"
Run-BuildScript "build_kexsmp_x86.ps1" "KexSmp"
Run-BuildScript "build_kexmls_x86.ps1" "KexMLS"

Run-BuildScript "build_kexdll_x86.ps1" "KexDll"
Copy-BuiltLib "KexDll" "KexDll"

Run-BuildScript "build_kexw32ml_x86.ps1" "KexW32ML"
Copy-BuiltLib "KexW32ML" "KexW32ML"

Run-BuildScript "build_kxcfghlp_x86.ps1" "KxCfgHlp"
Copy-BuiltLib "KxCfgHlp" "KxCfgHlp"

Run-BuildScript "build_kexgui_x86.ps1" "KexGui"
Copy-BuiltLib "KexGui" "KexGui"

Run-BuildScript "build_kxnt_x86.ps1" "KxNt"
Copy-BuiltLib "KxNt" "KxNt"

Run-BuildScript "build_kxbase_x86.ps1" "KxBase"
Copy-BuiltLib "KxBase" "KxBase"

Run-BuildScript "build_kexshlex_x86.ps1" "KexShlEx"
Copy-BuiltLib "KexShlEx" "KexShlEx"

Write-Host "`nWIN32 BUILD COMPLETED SUCCESSFULLY" -ForegroundColor Green
