param([Parameter(Mandatory=$true)][string]$BuildArchive,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$archive=(Get-Item -LiteralPath $BuildArchive).FullName
if(Test-Path -LiteralPath $Output){throw 'Preserve previous build receipt'}
$start=(Get-Item -LiteralPath $archive).CreationTimeUtc
$logs=@('all-x64-second.txt','all-x86-first.txt','kexdll-local-headers-x64.txt','kexdll-local-headers-x86.txt')
foreach($log in $logs){
 $content=Get-Content -LiteralPath (Join-Path $archive $log) -Raw
 if($content -match 'fatal error|error LNK|error C[0-9]{4}|BUILD FAILED'){throw "Compiler failure in $log"}
}
if((Get-Content "$archive\all-x64-second.txt" -Raw) -notmatch 'BUILD COMPLETED SUCCESSFULLY' -or
   (Get-Content "$archive\all-x86-first.txt" -Raw) -notmatch 'WIN32 BUILD COMPLETED SUCCESSFULLY'){throw 'Unified build did not reach its final phase'}
$modules=@('KexDll','KexShlEx','KxNt','KxBase','KxAdvapi','KxCom','KxCrt','KxCryp','KxDw','KxDx','KxMi','KxNet','KxSChanl','KxUia','KxUser')
$libraries=@('KexPathCch','KexSmp','KexMls','KexW32ML','KxCfgHlp','KexGui')
$files=@()
foreach($arch in @('x64','x86')){
 $platform=if($arch -eq 'x64'){'x64'}else{'Win32'}
 foreach($name in @($modules+$libraries)){
  $extension=if($name -in $modules){'dll'}else{'lib'}
  $relative="$platform\Release\$name\$name.$extension"
  $file=Get-Item -LiteralPath (Join-Path $root $relative)
  if($file.LastWriteTimeUtc -lt $start){throw "Stale build output: $relative"}
  $machine=$null
  if($extension -eq 'dll'){
   $bytes=[IO.File]::ReadAllBytes($file.FullName)
   if($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d){throw 'Missing DOS header'}
   $pe=[BitConverter]::ToInt32($bytes,60)
   if($pe -lt 64 -or $pe -gt $bytes.Length-24 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550){throw 'Invalid PE header'}
   $machine=[BitConverter]::ToUInt16($bytes,$pe+4)
   $expected=if($arch -eq 'x64'){0x8664}else{0x14c}
   if($machine -ne $expected -or !([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000)){throw "Wrong DLL architecture: $relative"}
  }
  $copy=Join-Path $archive "outputs\$relative"
  if(Test-Path -LiteralPath $copy){throw 'Preserve archived build output'}
  New-Item -ItemType Directory -Force (Split-Path $copy)|Out-Null
  Copy-Item -LiteralPath $file.FullName -Destination $copy
  $hash=(Get-FileHash $file.FullName).Hash
  if((Get-FileHash $copy).Hash -ne $hash){throw 'Archive hash mismatch'}
  $files+=[pscustomobject]@{Path=$relative;Architecture=$arch;Machine=$machine;Bytes=$file.Length;SHA256=$hash;ModifiedUTC=$file.LastWriteTimeUtc.ToString('o');FileVersion=$file.VersionInfo.FileVersion}
 }
}
$scripts=@('build_all.ps1','build_all_x86.ps1','build_kexdll.ps1','build_kexdll_x86.ps1','build_kexshlex.ps1','build_kexshlex_x86.ps1','build_kxbase_x86.ps1','tests\capture_kxnt_release_build_readiness.ps1')
[pscustomobject]@{State='UnifiedBuildReadinessVerified';Files=$files;Logs=@($logs|ForEach-Object {[pscustomobject]@{Path=$_;SHA256=(Get-FileHash (Join-Path $archive $_)).Hash}});Scripts=@($scripts|ForEach-Object {[pscustomobject]@{Path=$_;SHA256=(Get-FileHash (Join-Path $root $_)).Hash}});Limits='Build readiness only. Freshness, PE architecture and archive hashes verified for 30 DLLs and 12 static libraries. No Installer replacement, version bump, VM behavior or release publication is implied. First x64 failed log remains in the archive.'}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $Output -Encoding UTF8
