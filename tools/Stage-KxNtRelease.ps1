param([Parameter(Mandatory=$true)][string]$PackageDirectory,
      [Parameter(Mandatory=$true)][string]$BuildDirectory,
      [Parameter(Mandatory=$true)][ValidatePattern('^2\.0\.0\.[0-9]+$')][string]$Version)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$package=[IO.Path]::GetFullPath($PackageDirectory)
$build=[IO.Path]::GetFullPath($BuildDirectory)
if((Test-Path $package) -or (Test-Path $build)){throw 'Use fresh package and build directories'}
if($build -eq $package -or $build.StartsWith($package.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Keep logs outside the package'}
$header=[IO.File]::ReadAllText("$root\00-Common-Headers\vautogen.h")
$parts=$Version.Split('.')
$packed='0x{0:X8}' -f (0x80000000L+[long]$parts[3])
if($header -notmatch [regex]::Escape('"'+$Version+'"') -or
   $header -notmatch [regex]::Escape(($parts -join ',')) -or
   $header -notmatch [regex]::Escape($packed)){throw 'Version macros do not agree'}
New-Item -ItemType Directory $build|Out-Null
$started=[DateTime]::UtcNow
$logs=@()
function Build([string]$Script,[hashtable]$Arguments,[string]$Label){
 $path=Join-Path $root $Script
 if(!(Test-Path -LiteralPath $path)){throw "Missing build script: $Script"}
 $command="$ErrorActionPreference='Stop'; & '"+$path.Replace("'","''")+"'"
 # Use a literal preference assignment, not the parent value.
 $command='$ErrorActionPreference=''Stop''; & '''+$path.Replace("'","''")+''''
 foreach($key in $Arguments.Keys){$command+=" -$key '"+$Arguments[$key].ToString().Replace("'","''")+"'"}
 $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
 $stdout=Join-Path $build "$Label.txt";$stderr=Join-Path $build "$Label.stderr"
 $process=Start-Process "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',$encoded) -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 $process.WaitForExit();$exit=$process.ExitCode;$process.Dispose()
 $logs+=@($stdout,$stderr)
 if($exit -or (([IO.File]::ReadAllText($stdout)+[IO.File]::ReadAllText($stderr)) -match 'fatal error|error LNK|error C[0-9]{4}|BUILD FAILED')){throw "Build failed: $Label ($exit)"}
 Write-Host "Built $Label"
}
$replacements=@()
function Stage([string]$Source,[string]$Relative,[int]$Machine,[bool]$CheckVersion=$true){
 $file=Get-Item -LiteralPath $Source
 if($file.LastWriteTimeUtc -lt $started){throw "Stale output: $Source"}
 $bytes=[IO.File]::ReadAllBytes($file.FullName)
 if($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d){throw 'Invalid DOS header'}
 $pe=[BitConverter]::ToInt32($bytes,60)
 if($pe -lt 64 -or $pe -gt $bytes.Length-24 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes,$pe+4) -ne $Machine){throw "Invalid PE machine: $Relative"}
 $actualVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName).FileVersion
 if($CheckVersion -and $actualVersion -ne $Version){throw "Wrong version for $Relative : '$actualVersion'"}
 $destination=Join-Path $package $Relative
 New-Item -ItemType Directory -Force (Split-Path $destination)|Out-Null
 Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
 $hash=(Get-FileHash $file.FullName).Hash
 if((Get-FileHash $destination).Hash -ne $hash){throw 'Staging hash mismatch'}
 $script:replacements+=[pscustomobject]@{Path=$Relative;Source=$file.FullName;SHA256=$hash;Version=$actualVersion;Machine=$Machine}
}
Build 'build_all.ps1' @{} 'all-x64'
Build 'build_all_x86.ps1' @{} 'all-x86'
$installer=Join-Path $root 'Installer'
if(Get-ChildItem $installer -Recurse -Force|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Installer contains a reparse point'}
New-Item -ItemType Directory $package|Out-Null
Get-ChildItem $installer -Force|ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $package -Recurse -Force}
$dlls=@('KexDll','KxNt','KxBase','KxAdvapi','KxCom','KxCrt','KxCryp','KxDw','KxDx','KxMi','KxNet','KxSChanl','KxUia','KxUser')
foreach($arch in @('x64','x86')){
 $platform=if($arch -eq 'x64'){'x64'}else{'Win32'}
 $machine=if($arch -eq 'x64'){0x8664}else{0x14c}
 $prefix=if($arch -eq 'x64'){''}else{'Kex32\'}
 foreach($name in $dlls){Stage "$root\$platform\Release\$name\$name.dll" "$prefix$name.dll" $machine}
 foreach($name in @('KexCfg','VxlView')){
  $out=Join-Path $build "$arch\$name"
  Build "build_$($name.ToLower()).ps1" @{Architecture=$arch;OutputDirectory=$out} "$name-$arch"
  Stage "$out\$name.exe" "$prefix$name.exe" $machine
 }
}
Stage "$root\x64\Release\KexShlEx\KexShlEx.dll" 'KexShlEx.dll' 0x8664
foreach($entry in @(@('build_vistasetup.ps1','VistaSetup'),@('tools\VistaRun\build.ps1','VistaRun'))){
 $out=Join-Path $build "x64\$($entry[1])"
 Build $entry[0] @{OutputDirectory=$out} $entry[1]
 Stage "$out\$($entry[1]).exe" "$($entry[1]).exe" 0x8664 ($entry[1] -ne 'VistaRun')
}
$files=@(Get-ChildItem $package -Recurse -File|Sort-Object FullName|ForEach-Object {[pscustomobject]@{Path=$_.FullName.Substring($package.Length+1);Bytes=$_.Length;SHA256=(Get-FileHash $_.FullName).Hash}})
$sourcePaths=@(git -C $root ls-files -- '*.c' '*.h' '*.asm' '*.def' '*.rc' '*.ps1' '*.cpp' '*.manifest')
$sources=@($sourcePaths|ForEach-Object {if(Test-Path "$root\$_"){[pscustomobject]@{Path=$_;SHA256=(Get-FileHash "$root\$_").Hash}}})
[pscustomobject]@{State='StagedPendingRuntimeVerification';Version=$Version;StartedUTC=$started.ToString('o');Package=$package;Replacements=$replacements;Files=$files;Sources=$sources;Limits='Third-party dwrw10 / winpty and unchanged VistaPty assets retained byte-for-byte. VistaRun has no version resource; primary setup/configuration/core/extension DLL versions checked. No runtime success or publication is implied.'}|ConvertTo-Json -Depth 6|Set-Content "$build\manifest.json" -Encoding UTF8
