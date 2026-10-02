param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser = 'Administrator',
    [string]$GuestDirectory = 'C:\KxNtParity',
    [ValidateSet('processor-feature','domain','device-family','persisted-state')][string]$Probe = 'processor-feature',
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
    $exe = "$results\$arch\$Probe.exe"
    if (!(Test-Path $exe)) { throw "Build probes first: tests/build_kxnt_probes.ps1 -Architecture $arch" }
    Invoke-GuestTool @('copyFileFromHostToGuest', $VMX, $exe, "$guest\$Probe.exe")
    Invoke-GuestTool @('runProgramInGuest', $VMX, "$guest\$Probe.exe", "$guest\KxNt.dll", "$guest\$Probe.txt")
    $log = "$results\$arch\$Probe.txt"
    Invoke-GuestTool @('copyFileFromGuestToHost', $VMX, "$guest\$Probe.txt", $log)
    $text = [IO.File]::ReadAllText($log)
    if ($text -notmatch 'Result=PASS') {
        throw "Verification failed ($Probe/$arch): $text"
    }
    if ($Probe -eq 'processor-feature' -and $text -notmatch 'ValidFeatureChecks=64 InvalidFeatureChecks=5 Failures=0') {
        throw "Processor feature checks missing ($arch)"
    }
    if ($Probe -eq 'device-family' -and ($text -notmatch 'OptionalOutputCombinations=8 Failures=0' -or $text -notmatch 'SpoofedVersionCases=3')) {
        throw "Device family checks missing ($arch)"
    }
    if ($Probe -eq 'domain' -or $Probe -eq 'persisted-state') {
        $reference = Get-Content -Raw "$root\docs\validation\kxnt-$Probe-reference.json" | ConvertFrom-Json
        $lines = @($text -split '\r?\n' | Where-Object { $_ -and $_ -notmatch '^ProcessBits=' })
        $difference = Compare-Object @($reference.Output) $lines
        if ($difference) { throw "$Probe output differs from native RTL reference ($arch): $($difference | Out-String)" }
    }
    $receipt += [pscustomobject]@{
        Architecture = $arch
        BinarySHA256 = $hashes
        ProbeSHA256 = (Get-FileHash -Algorithm SHA256 $exe).Hash
        Output = $text
    }
    Write-Host "$arch ${Probe}: PASS"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\$Probe-receipt.json"
