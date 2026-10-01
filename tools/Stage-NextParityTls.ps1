param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [Parameter(Mandatory=$true)][string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
function Get-Sha256([string]$Path) {
    $stream=[IO.File]::OpenRead($Path); $sha=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$root = Split-Path $PSScriptRoot
$package = [IO.Path]::GetFullPath($PackageDirectory)
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (Test-Path -LiteralPath $package) { throw 'Use a new package directory; existing packages are never overwritten.' }
if ($package.TrimEnd('\') -eq $build.TrimEnd('\') -or
    $build.StartsWith($package.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build output must be outside the staged package.'
}
$installer = Join-Path $root 'Installer'
$bundle = Join-Path $root 'VxKex_NEXT_Source_Code_1_2_3_2463\02-Prebuilt Data\KexDir\Certificates\ROOT.sst'
if (!(Test-Path -LiteralPath $bundle -PathType Leaf)) { throw 'NEXT certificate bundle is missing.' }
$sourceFiles = @(Get-ChildItem -LiteralPath $installer -Recurse -Force)
if ($sourceFiles | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw 'Installer source contains a reparse point.'
}
New-Item -ItemType Directory -Force -Path $build | Out-Null
$native = Join-Path $build 'x64'
$wow = Join-Path $build 'x86'
function Invoke-Build([string]$Script, [hashtable]$Arguments, [string]$Log) {
    $command = "& '" + $Script.Replace("'", "''") + "'"
    foreach ($key in $Arguments.Keys) {
        $value = $Arguments[$key]
        if ($value -is [bool]) { if ($value) { $command += " -$key" }; continue }
        $quoted = @($value | ForEach-Object { "'" + $_.ToString().Replace("'", "''") + "'" })
        $command += " -$key " + $(if ($quoted.Count -gt 1) { '@(' + ($quoted -join ',') + ')' } else { $quoted[0] })
    }
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    # OS file redirection avoids legacy PowerShell's stderr/CLIXML pipeline,
    # which can block compiler writes after repeated diagnostic messages.
    $launch = @{
        FilePath='powershell.exe'; WindowStyle='Hidden'; PassThru=$true;
        ArgumentList=@('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded);
        RedirectStandardOutput=(Join-Path $build $Log);
        RedirectStandardError=(Join-Path $build ($Log + '.stderr'))
    }
    $process = Start-Process @launch
    $process.WaitForExit()
    $code = $process.ExitCode
    $process.Dispose()
    if ($code) { throw "Build failed; inspect $Log and $Log.stderr (exit $code)" }
}
Invoke-Build (Join-Path $root 'build_kexdll.ps1') @{OutputDirectory=(Join-Path $native 'KexDll')} 'kexdll-x64.txt'
Invoke-Build (Join-Path $root 'build_kexdll_x86.ps1') @{OutputDirectory=(Join-Path $wow 'KexDll')} 'kexdll-x86.txt'
Invoke-Build (Join-Path $root 'build_extended.ps1') @{Only=@('KxAdvapi','KxCryp','KxSChanl'); OutputRoot=$native; NoStage=$true} 'extended-x64.txt'
Invoke-Build (Join-Path $root 'build_extended_x86.ps1') @{Only=@('KxAdvapi','KxCryp','KxSChanl'); OutputRoot=$wow} 'extended-x86.txt'
function Assert-Machine([string]$Path, [int]$Machine) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) { throw "Invalid DOS header: $Path" }
    $offset = [BitConverter]::ToInt32($bytes, 60)
    if ($offset -lt 64 -or $offset -gt $bytes.Length - 24 -or
        [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes, $offset + 4) -ne $Machine -or
        !([BitConverter]::ToUInt16($bytes, $offset + 22) -band 0x2000)) { throw "Invalid DLL architecture: $Path" }
}
$replacements = @()
foreach ($name in @('KexDll','KxAdvapi','KxCryp','KxSChanl')) {
    foreach ($arch in @('x64','x86')) {
        $source = Join-Path (Join-Path $build $arch) "$name\$name.dll"
        Assert-Machine $source $(if ($arch -eq 'x64') { 0x8664 } else { 0x14c })
        $relative = $(if ($arch -eq 'x64') { "$name.dll" } else { "Kex32\$name.dll" })
        $replacements += [pscustomobject]@{Source=$source; Path=$relative; SHA256=(Get-Sha256 $source)}
    }
}
New-Item -ItemType Directory -Path $package | Out-Null
foreach ($entry in Get-ChildItem -LiteralPath $installer -Force) {
    Copy-Item -LiteralPath $entry.FullName -Destination $package -Recurse -Force
}
foreach ($entry in $replacements) {
    Copy-Item -LiteralPath $entry.Source -Destination (Join-Path $package $entry.Path) -Force
}
New-Item -ItemType Directory -Force -Path (Join-Path $package 'Certificates') | Out-Null
Copy-Item -LiteralPath $bundle -Destination (Join-Path $package 'Certificates\ROOT.sst') -Force
$files = @(Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName | ForEach-Object {
    [pscustomobject]@{Path=$_.FullName.Substring($package.Length+1); SHA256=(Get-Sha256 $_.FullName); Bytes=$_.Length}
})
[pscustomobject]@{
    Status='Staged; runtime verification required'; Package=$package; Build=$build;
    RootBundleSource=$bundle; RootBundleSHA256=(Get-Sha256 $bundle);
    Replacements=$replacements; Files=$files
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $build 'package-manifest.json') -Encoding UTF8
Write-Host "Staged $($files.Count) files in $package. No installed VM or Installer files were replaced."
