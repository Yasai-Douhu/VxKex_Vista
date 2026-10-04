param([Parameter(Mandatory=$true)][string]$Manifest,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$m=Get-Content -LiteralPath $Manifest -Raw|ConvertFrom-Json
if($m.State -ne 'StagedPendingRuntimeVerification' -or !$m.Files.Count -or $m.Replacements.Count -ne 35){throw 'Unexpected staging manifest'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh release archive'}
$package=[IO.Path]::GetFullPath($m.Package)
$actual=@(Get-ChildItem -LiteralPath $package -Recurse -File)
if($actual.Count -ne $m.Files.Count){throw 'Candidate file count changed'}
foreach($file in $m.Files){
 if($file.Path -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($file.Path)){throw 'Unsafe package-relative path'}
 foreach($directory in @($package,(Join-Path $root 'Installer'))){
  $path=Join-Path $directory $file.Path
  if((Get-FileHash -LiteralPath $path).Hash -ne $file.SHA256 -or (Get-Item -LiteralPath $path).Length -ne $file.Bytes){throw "Candidate byte mismatch: $path"}
 }
}
New-Item -ItemType Directory $output|Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $output "VxKex_Vista-$($m.Version).zip"
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 $entries=@($archive.Entries|Where-Object {$_.Name})
 if($entries.Count -ne $m.Files.Count){throw 'ZIP file count mismatch'}
 foreach($file in $m.Files){
  $name=$file.Path.Replace('\','/')
  $entry=@($entries|Where-Object {$_.FullName.Replace('\','/') -ceq $name})
  if($entry.Count -ne 1 -or $entry[0].Length -ne $file.Bytes){throw "ZIP entry mismatch: $name"}
  $stream=$entry[0].Open();$sha=[Security.Cryptography.SHA256]::Create()
  try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$sha.Dispose();$stream.Dispose()}
  if($hash -ne $file.SHA256){throw "ZIP round-trip hash mismatch: $name"}
 }
}finally{$archive.Dispose()}
$hash=(Get-FileHash -LiteralPath $zip).Hash
$checksum=Join-Path $output "VxKex_Vista-$($m.Version)-SHA256.txt"
[IO.File]::WriteAllText($checksum,$hash.ToLower()+'  '+[IO.Path]::GetFileName($zip)+"`r`n",[Text.Encoding]::ASCII)
[pscustomobject]@{State='PackagedRoundTripVerified';Version=$m.Version;ManifestSHA256=(Get-FileHash $Manifest).Hash;Files=$m.Files.Count;ZIP=[pscustomobject]@{Path=$zip;Bytes=(Get-Item $zip).Length;SHA256=$hash};Checksum=[pscustomobject]@{Path=$checksum;Bytes=(Get-Item $checksum).Length;SHA256=(Get-FileHash $checksum).Hash};Scope='Every candidate and current Installer file matches staging manifest; every ZIP file verifies by entry size and decompressed SHA256. Runtime verification and external upload must be verified separately.'}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $output 'receipt.json') -Encoding UTF8
