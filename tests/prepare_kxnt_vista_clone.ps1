$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$sourceDir='C:\Users\YamaR\Documents\Virtual Machines\Vista'
$destination='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Vista-Next-Parity-Test'
$archive="$root\audit\KxNtParity\vista-clone-20261004"
if((Test-Path $destination) -or (Test-Path $archive)){throw 'Preserve existing destination or evidence; do not overwrite'}
New-Item -ItemType Directory $destination,$archive|Out-Null
$sourceVMX="$sourceDir\Windows Vista x64 Edition.vmx";$snapshot="$sourceDir\Windows Vista x64 Edition.vmsd"
function Descriptor([string]$path){
 $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 try{
  $header=New-Object byte[] 512;if($stream.Read($header,0,512) -ne 512 -or [Text.Encoding]::ASCII.GetString($header,0,4) -ne 'KDMV'){throw 'Require sparse VMDK header'}
  $offset=[BitConverter]::ToUInt64($header,28)*512;$length=[BitConverter]::ToUInt64($header,36)*512
  if(!$length -or $length -gt 1048576 -or $offset+$length -gt $stream.Length){throw 'Invalid embedded descriptor range'}
  $stream.Position=$offset;$data=New-Object byte[] $length;if($stream.Read($data,0,$data.Length) -ne $data.Length){throw 'Short VMDK descriptor'}
  return [Text.Encoding]::ASCII.GetString($data).TrimEnd([char]0)
 }finally{$stream.Dispose()}
}
$state='Incomplete';$exitCode=$null
try{
 $snapText=[IO.File]::ReadAllText($snapshot);$vmText=[IO.File]::ReadAllText($sourceVMX)
 if($snapText -notmatch '(?m)^snapshot0.displayName = "CleanEnv_1"\r?$' -or $snapText -notmatch '(?m)^snapshot0.disk0.fileName = "Vista.vmdk"\r?$' -or $snapText -notmatch '(?m)^snapshot.numSnapshots = "1"\r?$' -or $vmText -notmatch '(?m)^scsi0:0.fileName = "Vista-000002.vmdk"\r?$'){throw 'Snapshot/current disk mapping changed; inspect before cloning'}
 $sourceDisk="$sourceDir\Vista.vmdk";$currentDisk="$sourceDir\Vista-000002.vmdk"
 $base=Descriptor $sourceDisk;$current=Descriptor $currentDisk
 $cid=[regex]::Match($base,'(?m)^CID=([0-9a-f]+)\r?$').Groups[1].Value
 if(!$cid -or $current -notmatch "(?m)^parentCID=$cid\r?`$" -or $current -notmatch '(?m)^parentFileNameHint="Vista.vmdk"\r?$' -or $base -match '(?m)^parentFileNameHint='){throw 'Require direct snapshot base, not a live delta or external parent'}
 $before=Get-Item $sourceDisk
 $targetDisk="$destination\Vista.vmdk"
 & 'C:\Program Files\VMware\VMware Workstation\vmware-vdiskmanager.exe' -r $sourceDisk -t 0 $targetDisk *> "$archive\disk-copy.txt"
 $exitCode=$LASTEXITCODE
 if($exitCode -or !(Test-Path $targetDisk)){throw 'Independent disk conversion failed; source VM remains unchanged'}
 if((Descriptor $sourceDisk) -cne $base -or (Get-Item $sourceDisk).LastWriteTimeUtc.Ticks -ne $before.LastWriteTimeUtc.Ticks -or [IO.File]::ReadAllText($snapshot) -cne $snapText){throw 'Snapshot source metadata changed during copy'}
 $converted=Descriptor $targetDisk;if($converted -match '(?m)^parentFileNameHint='){throw 'Converted disk still has a parent dependency'}
 $keys=@('config.version','virtualHW.version','guestOS','scsi0.present','scsi0.virtualDev')
 $lines=@('.encoding = "UTF-8"')
 foreach($key in $keys){$m=[regex]::Match($vmText,'(?m)^'+[regex]::Escape($key)+' = "[^"]+"\r?$');if(!$m.Success){throw "Missing source hardware key $key"};$lines+=$m.Value.TrimEnd("`r")}
 $lines+=@('displayName = "VxKex Vista NEXT parity setup test"','memsize = "4096"','numvcpus = "2"','scsi0:0.present = "TRUE"','scsi0:0.fileName = "Vista.vmdk"','ethernet0.present = "FALSE"','usb.present = "FALSE"','sound.present = "FALSE"','floppy0.present = "FALSE"','uuid.action = "create"')
 [IO.File]::WriteAllText("$destination\Vista-SetupParity.vmx",($lines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
 $state='DiskPrepared'
}finally{
 [pscustomobject]@{State=$state;SourceVMX=$sourceVMX;SourceSnapshot='CleanEnv_1';SourceSnapshotMetadata=$snapText;SourceBaseDescriptor=$base;SourceCurrentDescriptor=$current;Destination=$destination;DiskCopyExit=$exitCode;ConvertedDescriptor=$converted;PreparationSourceSHA256=(Get-FileHash $PSCommandPath).Hash;Limits='Disk-only cold clone from the existing snapshot base; no saved memory state, boot, guest credentials, native OS or IFEO behavior is proven by preparation. Never power on a VM referencing the user disk. No shutdown, suspend, snapshot restore, consolidation or lock removal is used.'}|ConvertTo-Json -Depth 5|Set-Content "$archive\receipt.json" -Encoding UTF8
}
