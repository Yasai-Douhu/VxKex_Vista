param(
 [Parameter(Mandatory=$true)][string]$VMX,
 [Parameter(Mandatory=$true)][string]$GuestUser,
 [Parameter(Mandatory=$true)][string]$GuestPassword,
 [Parameter(Mandatory=$true)][string]$GuestDirectory,
 [Parameter(Mandatory=$true)][string[]]$Names,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName,
 [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop';$root=Split-Path $PSScriptRoot
$archive="$root\audit\KxNtParity\imports-$RunName"
if(Test-Path $archive){throw 'Preserve previous import audit'}
if($GuestDirectory -notmatch '^[A-Za-z]:\\' -or $Names.Count -eq 0 -or @($Names|Select-Object -Unique).Count -ne $Names.Count){throw 'Explicit unique guest filenames required'}
foreach($name in $Names){if($name -notmatch '^[a-zA-Z0-9_.-]+\.(dll|exe)$'){throw 'Only literal filenames allowed'}}
$baseline="$root\audit\KxNtParity\imports-priority-20261004"
New-Item -ItemType Directory $archive|Out-Null
$copies=@();$apps=@();$state='Incomplete'
try{
 foreach($name in $Names){
  $guest="$($GuestDirectory.TrimEnd('\'))\$name";$hostPath="$archive\$name"
  & $VMRun -T ws -gu $GuestUser -gp $GuestPassword copyFileFromGuestToHost $VMX $guest $hostPath *> "$archive\copy-$name.txt"
  if($LASTEXITCODE){throw "Read-only guest copy failed for $name; inspect saved log before retry"}
  $copies+=[pscustomobject]@{GuestPath=$guest;HostPath=$hostPath;SHA256=(Get-FileHash $hostPath).Hash;Bytes=(Get-Item $hostPath).Length}
  $apps+=@('--app',$hostPath)
 }
 & 'C:\Program Files\PyManager\python.exe' "$PSScriptRoot\audit_kxnt_application_imports.py" --native-x86 "$baseline\native-server-x86.dll" --native-x64 "$baseline\native-server-x64.dll" --kxnt-x86 "$root\Installer\Kex32\KxNt.dll" --kxnt-x64 "$root\Installer\KxNt.dll" @apps --watch-name NtQuerySystemInformationEx --watch-name NtQueryWnfStateData --watch-name RtlUnsubscribeWnfNotificationWaitForCompletion --output "$archive\report.json" *> "$archive\audit.txt"
 if($LASTEXITCODE){throw 'Import scanner failed; preserve evidence'}
 $state='StaticAuditCompleted'
}finally{
 [pscustomobject]@{State=$state;VMX=$VMX;GuestDirectory=$GuestDirectory;Copies=$copies;CollectorSHA256=(Get-FileHash $PSCommandPath).Hash;ScannerSHA256=(Get-FileHash "$PSScriptRoot\audit_kxnt_application_imports.py").Hash;Scope='Explicit files copied read-only from running guest. No guest program execution, writes, installation or registry changes. Cached native baseline hashes recorded by scanner; not current system-file freshness, recursive complete dependency inventory, dynamic call or runtime behavior proof.'}|ConvertTo-Json -Depth 5|Set-Content "$archive\receipt.json" -Encoding UTF8
}
