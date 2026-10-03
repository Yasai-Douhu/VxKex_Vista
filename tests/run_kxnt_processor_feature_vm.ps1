param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser = 'Administrator',
    [string]$GuestDirectory = 'C:\KxNtParity',
    [ValidateSet('processor-feature','domain','device-family','persisted-state','sid-package','sid-capability','membership','compare','file-information','alert')][string]$Probe = 'processor-feature',
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
    $exeName = if ($Probe -like 'sid-*') { 'sid-class' } else { $Probe }
    $exe = "$results\$arch\$exeName.exe"
    if (!(Test-Path $exe)) { throw "Build probes first: tests/build_kxnt_probes.ps1 -Architecture $arch" }
    Invoke-GuestTool @('copyFileFromHostToGuest', $VMX, $exe, "$guest\$Probe.exe")
    $run = @('runProgramInGuest', $VMX, "$guest\$Probe.exe", "$guest\KxNt.dll", "$guest\$Probe.txt")
    if ($Probe -eq 'sid-package') { $run += 'RtlIsPackageSid' }
    if ($Probe -eq 'sid-capability') { $run += 'RtlIsCapabilitySid' }
    if ($Probe -eq 'alert') { $run += 'CoreOnly' }
    Invoke-GuestTool $run
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
    if ($Probe -eq 'compare') {
        $before = @($text -split '\r?\n' | Where-Object { $_ -match '^Diagnostic before ' } | ForEach-Object { $_ -replace '^Diagnostic before ','' })
        $after = @($text -split '\r?\n' | Where-Object { $_ -match '^Diagnostic after ' } | ForEach-Object { $_ -replace '^Diagnostic after ','' })
        if (!$before.Count -or !$after.Count -or (Compare-Object $before $after)) {
            throw "Comparison changed live handle identities/types during measured calls ($arch)"
        }
    }
    if ($Probe -eq 'file-information' -and ($text -notmatch 'FileInformationCases=96 Failures=0' -or ([regex]::Matches($text,'RepeatCalls=1000 HandleDelta=0')).Count -ne 2)) {
        throw "File information checks missing ($arch)"
    }
    if ($Probe -eq 'alert') {
        $references = Get-Content -Raw "$root\docs\validation\kxnt-alert-reference.json" | ConvertFrom-Json
        $reference = @($references | Where-Object { $_.Architecture -eq $arch })
        if ($reference.Count -ne 1) { throw "Missing or ambiguous thread alert reference ($arch)" }
        $nativeLines = @($reference.HostOutput -split '\r?\n' | Where-Object { $_ -and $_ -notmatch '^ProcessBits=|^Diagnostic |^TerminatedWaiterTest ' })
        $actualLines = @($text -split '\r?\n' | Where-Object { $_ -and $_ -notmatch '^ProcessBits=|^Diagnostic ' })
        if (Compare-Object $nativeLines $actualLines) { throw "Thread alert differs from native reference ($arch)" }
        # Launch the isolated termination case directly through VMware Tools.
        # A WOW64 parent that loads KexDll enables propagation and can inject
        # the older SYSTEM copy; that is a separate deployment integration test.
        $termination = @{}
        foreach ($alias in @('0','1')) {
            $guestLog = "$guest\alert-termination-$alias.txt"
            Invoke-GuestTool @('runProgramInGuest', $VMX, "$guest\$Probe.exe", '--termination', "$guest\KxNt.dll", $guestLog, $alias)
            $terminationLog = "$results\$arch\alert-termination-$alias.txt"
            Invoke-GuestTool @('copyFileFromGuestToHost', $VMX, $guestLog, $terminationLog)
            $terminationText = [IO.File]::ReadAllText($terminationLog)
            if ($terminationText -notmatch 'Result=PASS' -or $terminationText -notmatch 'CurrentStatus=00000102 HandleDelta=0') {
                throw "Terminated waiter state not reclaimed ($arch/$alias): $terminationText"
            }
            $termination[$alias] = $terminationText
        }
    }
    if ($Probe -eq 'domain' -or $Probe -eq 'persisted-state' -or $Probe -like 'sid-*' -or $Probe -eq 'membership' -or $Probe -eq 'compare') {
        $reference = Get-Content -Raw "$root\docs\validation\kxnt-$Probe-reference.json" | ConvertFrom-Json
        $lines = @($text -split '\r?\n' | Where-Object { $_ -and $_ -notmatch '^ProcessBits=|^Diagnostic ' })
        $difference = Compare-Object @($reference.Output) $lines
        if ($difference) { throw "$Probe output differs from native RTL reference ($arch): $($difference | Out-String)" }
    }
    $row = [pscustomobject]@{
        Architecture = $arch
        BinarySHA256 = $hashes
        ProbeSHA256 = (Get-FileHash -Algorithm SHA256 $exe).Hash
        Output = $text
    }
    if ($Probe -eq 'alert') { $row | Add-Member -NotePropertyName TerminationOutput -NotePropertyValue $termination }
    $receipt += $row
    Write-Host "$arch ${Probe}: PASS"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\$Probe-receipt.json"
