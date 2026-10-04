param([Parameter(Mandatory=$true)][ValidatePattern('^[a-z0-9-]+$')][string]$RunName)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$vmrun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
$vmx='C:\Users\YamaR\Documents\Virtual Machines\VxKex-Vista-Current-Parity-Test\Vista-CurrentParity.vmx'
$guest="C:\VxKexProbe\CurrentBaseline-$RunName"
$archive="$root\audit\KxNtParity\vista-current-baseline-$RunName"
if(Test-Path $archive){throw 'Preserve earlier baseline evidence'}
function Guest([string[]]$arguments){& $vmrun -T ws -gu Vista -gp 1111 @arguments;if($LASTEXITCODE){throw "VMware failed: $($arguments[0])"}}
& $vmrun -T ws -gu Vista -gp 1111 directoryExistsInGuest $vmx $guest
if(!$LASTEXITCODE){throw 'Do not overwrite an existing guest baseline'}
New-Item -ItemType Directory $archive|Out-Null
Guest @('createDirectoryInGuest',$vmx,$guest)
$script=@'
@echo off
ver > C:\VxKexProbe\CurrentBaseline\inventory.txt
whoami /user >> C:\VxKexProbe\CurrentBaseline\inventory.txt
whoami /priv >> C:\VxKexProbe\CurrentBaseline\inventory.txt
reg query HKLM\Software\VXsoft\VxKex /s >> C:\VxKexProbe\CurrentBaseline\inventory.txt 2>&1
reg export HKLM\Software\VXsoft\VxKex C:\VxKexProbe\CurrentBaseline\machine.reg /y > C:\VxKexProbe\CurrentBaseline\exports.txt 2>&1
if errorlevel 1 exit /b 10
reg export "HKLM\Software\Microsoft\Windows NT\CurrentVersion\Image File Execution Options" C:\VxKexProbe\CurrentBaseline\ifeo.reg /y >> C:\VxKexProbe\CurrentBaseline\exports.txt 2>&1
if errorlevel 1 exit /b 11
reg query HKCU\Software\VXsoft /s >> C:\VxKexProbe\CurrentBaseline\inventory.txt 2>&1
reg export HKCU\Software\VXsoft C:\VxKexProbe\CurrentBaseline\user.reg /y >> C:\VxKexProbe\CurrentBaseline\exports.txt 2>&1
echo UserExportExit=%errorlevel% >> C:\VxKexProbe\CurrentBaseline\exports.txt
dir C:\VxKex /s /b >> C:\VxKexProbe\CurrentBaseline\inventory.txt 2>&1
exit /b 0
'@
$script=$script.Replace('C:\VxKexProbe\CurrentBaseline',$guest)
[IO.File]::WriteAllText("$archive\inventory.cmd",$script.Replace("`n","`r`n"),[Text.ASCIIEncoding]::new())
Guest @('copyFileFromHostToGuest',$vmx,"$archive\inventory.cmd","$guest\inventory.cmd")
Guest @('copyFileFromHostToGuest',$vmx,"$root\audit\KxNtParity\x64\native-launch.exe","$guest\native-launch.exe")
Guest @('runProgramInGuest',$vmx,"$guest\native-launch.exe",'C:\Windows\System32\cmd.exe',"$guest\launch.txt","/d /c $guest\inventory.cmd")
foreach($name in @('launch.txt','inventory.txt','exports.txt','machine.reg','ifeo.reg')){Guest @('copyFileFromGuestToHost',$vmx,"$guest\$name","$archive\$name")}
$userExport=[IO.File]::ReadAllText("$archive\exports.txt") -match 'UserExportExit=0'
if($userExport){Guest @('copyFileFromGuestToHost',$vmx,"$guest\user.reg","$archive\user.reg")}
$launch=[IO.File]::ReadAllText("$archive\launch.txt")
if($launch -notmatch 'KexDllLoaded=0' -or $launch -notmatch 'ChildWait=00000000 ChildExitCode=00000000'){throw 'Baseline command did not finish successfully'}
$files=@()
foreach($arch in @('x64','x86')){
 $system=if($arch -eq 'x64'){'System32'}else{'SysWOW64'}
 foreach($name in @('KexDll.dll','KxNt.dll')){
  $local="$archive\$arch-$name"
  Guest @('copyFileFromGuestToHost',$vmx,"C:\Windows\$system\$name",$local)
  $files+=[pscustomobject]@{GuestPath="C:\Windows\$system\$name";SHA256=(Get-FileHash $local).Hash;Bytes=(Get-Item $local).Length}
 }
}
[pscustomobject]@{State='Captured';VMX=$vmx;GuestDirectory=$guest;NativeLaunchSHA256=(Get-FileHash "$root\audit\KxNtParity\x64\native-launch.exe").Hash;RegistryFiles=@('machine.reg','ifeo.reg');UserRegistryExported=$userExport;SystemFiles=$files;Scope='Read-only installed baseline capture on the independent current-state Vista clone; unavailable HKCU export is recorded, not proof of absence; no uninstall or API success is implied'}|ConvertTo-Json -Depth 5|Set-Content "$archive\receipt.json" -Encoding UTF8
