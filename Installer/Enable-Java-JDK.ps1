param(
    [Parameter(Mandatory=$true)][string]$JdkRoot,
    [string]$KexCfgPath
)
$ErrorActionPreference = 'Stop'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an elevated Windows PowerShell session to update IFEO settings.'
}

if (!(Test-Path -LiteralPath $JdkRoot -PathType Container)) {
    throw "JDK directory does not exist: $JdkRoot"
}

if (!$KexCfgPath) {
    $nextToScript = Join-Path $PSScriptRoot 'KexCfg.exe'
    if (Test-Path -LiteralPath $nextToScript) {
        $KexCfgPath = $nextToScript
    } else {
        $kexKey = Get-ItemProperty 'HKLM:\SOFTWARE\VXsoft\VxKex' -ErrorAction SilentlyContinue
        if ($kexKey -and $kexKey.KexDir) {
            $KexCfgPath = Join-Path $kexKey.KexDir 'KexCfg.exe'
        }
    }
}
if (!$KexCfgPath -or !(Test-Path -LiteralPath $KexCfgPath)) {
    throw 'KexCfg.exe was not found. Pass -KexCfgPath explicitly.'
}

$roots = @()
if (Test-Path -LiteralPath (Join-Path $JdkRoot 'release')) {
    $roots += (Get-Item -LiteralPath $JdkRoot).FullName
} else {
    foreach ($directory in Get-ChildItem -LiteralPath $JdkRoot) {
        if ($directory.PSIsContainer -and (Test-Path -LiteralPath (Join-Path $directory.FullName 'release'))) {
            $roots += $directory.FullName
        }
    }
}
if ($roots.Count -eq 0) { throw "No JDK release file found under $JdkRoot" }

$count = 0
foreach ($root in $roots) {
    $release = Get-Content -LiteralPath (Join-Path $root 'release')
    $versionLine = @($release | Where-Object {$_ -match '^JAVA_VERSION='})
    if ($versionLine.Count -ne 1 -or $versionLine[0] -notmatch '^JAVA_VERSION="?([0-9]+)') {
        throw "Cannot read JAVA_VERSION from $root\release"
    }
    $major = [int]$Matches[1]
    if ($major -lt 17) { throw "JDK $major is outside the Java 17+ target range: $root" }
    $bin = Join-Path $root 'bin'
    if (!(Test-Path -LiteralPath $bin -PathType Container)) { throw "Missing bin directory: $bin" }
    $executables = @(Get-ChildItem -LiteralPath $bin -Filter '*.exe' | Where-Object {!$_.PSIsContainer})
    if ($executables.Count -eq 0) { throw "No JDK launchers found in $bin" }

    Write-Output "JDK $major : $root ($($executables.Count) launchers)"
    foreach ($exe in $executables) {
        $start = New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName = $KexCfgPath
        $start.Arguments = '"/EXE:' + $exe.FullName + '" /ENABLE:1 /WINVERSPOOF:4'
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $process = [System.Diagnostics.Process]::Start($start)
        try {
            if (!$process.WaitForExit(30000)) {
                $process.Kill()
                throw "KexCfg.exe timed out for $($exe.FullName)"
            }
            if ($process.ExitCode -ne 0) {
                throw "KexCfg.exe failed for $($exe.FullName): exit $($process.ExitCode)"
            }
        } finally {
            $process.Dispose()
        }
        $count++
        Write-Output "Enabled: $($exe.FullName)"
    }
}
Write-Output "Configured $count JDK launchers."
