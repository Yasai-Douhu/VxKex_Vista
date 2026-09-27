# Build and package the runtime needed by WOW64 apps (the shell UI remains x64).
$ErrorActionPreference = 'Stop'
foreach ($script in @('build_kexpathcch_x86.ps1','build_kexsmp_x86.ps1','build_kexmls_x86.ps1','build_kexdll_x86.ps1','build_kexw32ml_x86.ps1','build_kexgui_x86.ps1','build_kxnt_x86.ps1','build_kxbase_x86.ps1','build_extended_x86.ps1','package_x86.ps1')) {
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot $script)
 if ($LASTEXITCODE) { throw "Build failed: $script" }
}
