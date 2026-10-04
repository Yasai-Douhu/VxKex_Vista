$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$vmrun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
$source='C:\Users\YamaR\Documents\Virtual Machines\Vista\Windows Vista x64 Edition.vmx'
$destination='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Vista-Current-Parity-Test\Vista-CurrentParity.vmx'
$archive="$root\audit\KxNtParity\vista-current-clone-20261004"
if((Test-Path (Split-Path $destination)) -or (Test-Path $archive)){throw 'Preserve previous clone and evidence; do not overwrite'}
$running=(& $vmrun -T ws list) -join "`n"
if($LASTEXITCODE -or $running -match [regex]::Escape($source)){throw 'Source must be powered off before a current-state full clone'}
$sourceHash=(Get-FileHash $source).Hash
New-Item -ItemType Directory $archive|Out-Null
$state='Incomplete';$exitCode=$null
try {
 & $vmrun -T ws clone $source $destination full '-cloneName=VxKex Vista current NEXT parity test' *> "$archive\clone.txt"
 $exitCode=$LASTEXITCODE
 if($exitCode -or !(Test-Path $destination)){throw 'Current-state full clone failed'}
 if((Get-FileHash $source).Hash -ne $sourceHash){throw 'Source VMX changed during clone; inspect before continuing'}
 $text=[IO.File]::ReadAllText($destination)
 $disk=[regex]::Match($text,'(?m)^scsi0:0.fileName = "([^"]+)"\r?$')
 if(!$disk.Success -or [IO.Path]::IsPathRooted($disk.Groups[1].Value) -or $disk.Groups[1].Value.Contains('..') -or $disk.Groups[1].Value.Contains('\')){throw 'Unexpected clone disk reference; inspect before boot'}
 if(!(Test-Path (Join-Path (Split-Path $destination) $disk.Groups[1].Value))){throw 'Cloned disk missing'}
 $state='ClonedNotBooted'
}finally{
 [pscustomobject]@{State=$state;SourceVMX=$source;SourceVMXSHA256=$sourceHash;SourcePowerVerifiedOff=$true;DestinationVMX=$destination;CloneExit=$exitCode;CloneMode='Full current state, no snapshot parameter';PreparationSourceSHA256=(Get-FileHash $PSCommandPath).Hash;Limits='User-authorized current-state clone. Previous disk-only clone is retained. Guest OS, credentials, installed baseline and IFEO behavior remain unverified.'}|ConvertTo-Json -Depth 4|Set-Content "$archive\receipt.json" -Encoding UTF8
}
