param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser = 'Administrator',
    [string]$GuestDirectory = 'C:\KxNtParity',
    [string]$VMRun = 'C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$results = "$root\audit\KxNtParity"
New-Item -ItemType Directory -Force $results | Out-Null
function Invoke-GuestTool([string[]]$Operation) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Operation
    if ($LASTEXITCODE) { throw "VMware operation failed: $($Operation[0]) ($LASTEXITCODE)" }
}
function Ensure-GuestDirectory([string]$Path) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword directoryExistsInGuest $VMX $Path | Out-Null
    if ($LASTEXITCODE) {
        Invoke-GuestTool @('createDirectoryInGuest', $VMX, $Path)
    }
}
# Execute the probe directly: no cmd.exe quoting or interactive UI is needed.
Ensure-GuestDirectory $GuestDirectory
$receipt = @()
foreach ($arch in @('x86','x64')) {
    $release = if ($arch -eq 'x86') { 'Win32' } else { 'x64' }
    $guest = "$GuestDirectory\$arch"
    Ensure-GuestDirectory $guest
    $hashes = @{}
    foreach ($dll in @('KexDll','KxNt')) {
        $source = "$root\$release\Release\$dll\$dll.dll"
        Invoke-GuestTool @('copyFileFromHostToGuest', $VMX, $source, "$guest\$dll.dll")
        $hashes[$dll] = (Get-FileHash -Algorithm SHA256 $source).Hash
    }
    $exe = "$results\$arch\processor-feature.exe"
    if (!(Test-Path $exe)) { throw "Build probes first: tests/build_kxnt_probes.ps1 -Architecture $arch" }
    Invoke-GuestTool @('copyFileFromHostToGuest', $VMX, $exe, "$guest\processor-feature.exe")
    Invoke-GuestTool @('runProgramInGuest', $VMX, "$guest\processor-feature.exe", "$guest\KxNt.dll", "$guest\processor-feature.txt")
    $log = "$results\$arch\processor-feature.txt"
    Invoke-GuestTool @('copyFileFromGuestToHost', $VMX, "$guest\processor-feature.txt", $log)
    $text = [IO.File]::ReadAllText($log)
    if ($text -notmatch 'Result=PASS' -or $text -notmatch 'ValidFeatureChecks=64 InvalidFeatureChecks=5 Failures=0') {
        throw "Processor feature verification failed ($arch): $text"
    }
    $receipt += [pscustomobject]@{
        Architecture = $arch
        BinarySHA256 = $hashes
        ProbeSHA256 = (Get-FileHash -Algorithm SHA256 $exe).Hash
        Output = $text
    }
    Write-Host "$arch processor feature: PASS"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\processor-feature-receipt.json"
