param([Parameter(Mandatory=$true)][string]$AppDirectory,[string]$PayloadDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
function Hash($path){$stream=[IO.File]::OpenRead($path);try{$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$sha.Dispose()}}finally{$stream.Dispose()}}
$dest=Join-Path $AppDirectory 'node_modules.asar.unpacked\node-pty\build\Release'
$target=Join-Path $dest 'conpty.node'
$backup=Join-Path $dest 'conpty.node.pre-vistapty'
if((Hash $backup) -ne '3E81F5A9868A6F41316AB2895ED965554B4DCB733DF1C06EB06DEA85100EAB98'){throw 'Backup does not match the supported original.'}
foreach($name in @('conpty.node','winpty.dll','winpty-agent.exe','vista-pty-clear.exe')){if((Hash (Join-Path $dest $name)) -ne (Hash (Join-Path $PayloadDirectory $name))){throw "Changed file $name; refusing to remove it"}}
# Close VS Code first. A mapped addon may otherwise prevent deletion.
Remove-Item -LiteralPath $target
try {Copy-Item -LiteralPath $backup -Destination $target}catch{Copy-Item -LiteralPath (Join-Path $PayloadDirectory 'conpty.node') -Destination $target;throw}
foreach($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe')){Remove-Item -LiteralPath (Join-Path $dest $name)}
# Retain the original backup for recovery; installation refuses an existing backup.
'Restored original node-pty. Original backup retained.'
