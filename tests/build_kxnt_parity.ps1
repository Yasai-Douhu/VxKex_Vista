# Build every dependency-changing step sequentially, rejecting stale binaries
# immediately when a compiler/linker fails.
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$logs = "$root\audit\KxNtParity"
New-Item -ItemType Directory -Force $logs | Out-Null
$steps = @(
    @{ File = 'build_kexdll.ps1'; Args = @(); Name = 'build-kexdll-x64' },
    @{ File = 'build_kexdll_x86.ps1'; Args = @(); Name = 'build-kexdll-x86' },
    @{ File = 'build_kxnt.ps1'; Args = @(); Name = 'build-kxnt-x64' },
    @{ File = 'build_kxnt_x86.ps1'; Args = @(); Name = 'build-kxnt-x86' },
    @{ File = 'tests/build_kxnt_probes.ps1'; Args = @('-Architecture','x64'); Name = 'probe-build-x64' },
    @{ File = 'tests/build_kxnt_probes.ps1'; Args = @('-Architecture','x86'); Name = 'probe-build-x86' }
)
foreach ($step in $steps) {
    Write-Host "Building $($step.Name)..."
    $arguments = $step.Args
    $log = "$logs\$($step.Name).log"
    # Classic PowerShell treats a compiler's stderr banner as an error record.
    # Let the native program finish, then use its actual exit code as the gate.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$root\$($step.File)" @arguments *> $log
        $buildExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($buildExit) {
        Get-Content $log -Tail 20 | Write-Host
        throw "Build failed: $($step.Name) (exit $buildExit); see $log"
    }
}
Write-Host 'KexDll, KxNt and probes built successfully for x86 and x64.'
