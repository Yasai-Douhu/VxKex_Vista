param([Parameter(Mandatory=$true)][string]$AppDirectory,[string]$PayloadDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
function Hash($path){$stream=[IO.File]::OpenRead($path);try{$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$sha.Dispose()}}finally{$stream.Dispose()}}
$dest=Join-Path $AppDirectory 'node_modules.asar.unpacked\node-pty\build\Release'
$target=Join-Path $dest 'conpty.node'
$backup=Join-Path $dest 'conpty.node.pre-vistapty'
$expected='3E81F5A9868A6F41316AB2895ED965554B4DCB733DF1C06EB06DEA85100EAB98'
if((Get-WmiObject Win32_OperatingSystem).Version -notlike '6.0.*'){throw 'This package targets NT 6.0 only.'}
if(((Get-Content (Join-Path $AppDirectory 'package.json')) -join "`n") -notmatch '"version"\s*:\s*"1\.139\.1"'){throw 'This package is verified only with VS Code 1.139.1.'}
if(Test-Path $backup){throw 'VistaPty backup already exists; inspect deployment before updating.'}
if((Hash $target) -ne $expected){throw 'Unsupported node-pty binary. No changes made.'}
foreach($name in @('conpty.node','winpty.dll','winpty-agent.exe','vista-pty-clear.exe')){if(!(Test-Path (Join-Path $PayloadDirectory $name))){throw "Missing payload $name"}}
foreach($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe')){if(Test-Path (Join-Path $dest $name)){throw "Existing file $name; refusing to overwrite"}}
Move-Item -LiteralPath $target -Destination $backup
try {
 foreach($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe','conpty.node')){Copy-Item -LiteralPath (Join-Path $PayloadDirectory $name) -Destination (Join-Path $dest $name)}
 if((Hash $target) -ne (Hash (Join-Path $PayloadDirectory 'conpty.node'))){throw 'Deployment hash mismatch'}
 'Installed VistaPty. Restart the VS Code terminal host to load it.'
}catch {
 if(Test-Path $target){Remove-Item -LiteralPath $target}
 Move-Item -LiteralPath $backup -Destination $target
 foreach($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe')){if(Test-Path (Join-Path $dest $name)){Remove-Item -LiteralPath (Join-Path $dest $name)}}
 throw
}
