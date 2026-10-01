param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [Parameter(Mandatory=$true)][string]$BuildDirectory
)
$ErrorActionPreference='Stop'
function Get-Sha256([string]$Path) {
    $stream=[IO.File]::OpenRead($Path); $sha=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$root=Split-Path $PSScriptRoot
$package=[IO.Path]::GetFullPath($PackageDirectory)
$build=[IO.Path]::GetFullPath($BuildDirectory)
if(Test-Path -LiteralPath $package) { throw 'Use a new package directory.' }
if(Test-Path -LiteralPath $build) { throw 'Use a new build directory to preserve provenance.' }
if($build.StartsWith($package.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or $build -eq $package) { throw 'Build must be outside the package.' }
# Start with the complete preserved Installer assets and freshly built TLS pair.
& "$PSScriptRoot\Stage-NextParityTls.ps1" -PackageDirectory $package -BuildDirectory $build
if(!$?) { throw 'TLS staging failed' }
function Invoke-Phase([string]$Script,[hashtable]$Arguments,[string]$Label) {
    $command="& '"+(Join-Path $root $Script).Replace("'","''")+"'"
    foreach($key in $Arguments.Keys) { $command+=" -$key '"+$Arguments[$key].ToString().Replace("'","''")+"'" }
    $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $process=Start-Process powershell.exe -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded) -RedirectStandardOutput (Join-Path $build "$Label.txt") -RedirectStandardError (Join-Path $build "$Label.stderr")
    $process.WaitForExit(); $code=$process.ExitCode; $process.Dispose()
    if($code) { throw "$Label failed (exit $code); inspect build logs." }
}
$replacements=@((Get-Content (Join-Path $build 'package-manifest.json') -Raw | ConvertFrom-Json).Replacements)
foreach($arch in @('x64','x86')) {
    $archDir=Join-Path $build $arch
    $helper=Join-Path $archDir 'KxCfgHlp'
    $helperScript=if($arch -eq 'x64'){'build_kxcfghlp.ps1'}else{'build_kxcfghlp_x86.ps1'}
    Invoke-Phase $helperScript @{OutputDirectory=$helper} "helper-$arch"
    $library=Join-Path $helper 'KxCfgHlp.lib'
    Invoke-Phase 'build_kexcfg.ps1' @{Architecture=$arch; OutputDirectory=(Join-Path $archDir 'KexCfg'); ConfigurationLibrary=$library} "settings-$arch"
    Invoke-Phase 'build_vxlview.ps1' @{Architecture=$arch; OutputDirectory=(Join-Path $archDir 'VxlView'); ConfigurationLibrary=$library} "viewer-$arch"
    foreach($name in @('KexCfg','VxlView')) {
        $source=Join-Path $archDir "$name\$name.exe"
        $relative=if($arch -eq 'x64'){"$name.exe"}else{"Kex32\$name.exe"}
        $bytes=[IO.File]::ReadAllBytes($source); $offset=[BitConverter]::ToInt32($bytes,60)
        $machine=if($arch -eq 'x64'){0x8664}else{0x14c}
        if([BitConverter]::ToUInt32($bytes,$offset) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$offset+4) -ne $machine -or ([BitConverter]::ToUInt16($bytes,$offset+22) -band 0x2000)) { throw "Invalid executable architecture: $source" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $package $relative) -Force
        $replacements+=[pscustomobject]@{Source=$source;Path=$relative;SHA256=(Get-Sha256 $source)}
    }
}
$setupDir=Join-Path $build 'x64\VistaSetup'
Invoke-Phase 'build_vistasetup.ps1' @{Architecture='x64';OutputDirectory=$setupDir;ConfigurationLibrary=(Join-Path $build 'x64\KxCfgHlp\KxCfgHlp.lib')} 'setup-x64'
$setup=Join-Path $setupDir 'VistaSetup.exe'
Copy-Item -LiteralPath $setup -Destination (Join-Path $package 'VistaSetup.exe') -Force
$replacements+=[pscustomobject]@{Source=$setup;Path='VistaSetup.exe';SHA256=(Get-Sha256 $setup)}
$files=@(Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object { [pscustomobject]@{Path=$_.FullName.Substring($package.Length+1);Bytes=$_.Length;SHA256=(Get-Sha256 $_.FullName)} })
$sources=@(foreach($module in @('KexDll','KxAdvapi','KxCryp','KxSChanl','KxCfgHlp','KexCfg','VxlView','VistaSetup','00-Common-Headers')) {
    Get-ChildItem (Join-Path $root $module) -File | Where-Object Extension -In '.c','.h','.asm','.def','.rc','.manifest' | ForEach-Object { [pscustomobject]@{Path=$_.FullName.Substring($root.Length+1);SHA256=(Get-Sha256 $_.FullName)} }
})
[pscustomobject]@{Status='Integrated candidate; final VM regression required';Package=$package;Build=$build;Replacements=$replacements;Files=$files;Sources=$sources} | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $build 'high-package-manifest.json') -Encoding UTF8
Write-Host "Integrated $($files.Count) files, $($replacements.Count) source-built replacements. Installer is unchanged pending final regression."
