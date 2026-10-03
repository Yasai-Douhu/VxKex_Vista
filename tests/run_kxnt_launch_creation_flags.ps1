param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [Parameter(Mandatory=$true)][string]$GuestUser,
    [Parameter(Mandatory=$true)][string]$GuestDirectory,
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$receipt=@()
function Guest([string[]]$Arguments){& $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments;if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}}
foreach($arch in @('x86','x64')){
    $results="$root\audit\KxNtParity\$arch";$guest="$GuestDirectory\$arch";$probe="$results\native-launch.exe"
    Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\native-launch.exe")
    foreach($mode in @('normal','no-console')){
        $operation=@('runProgramInGuest',$VMX,"$guest\native-launch.exe",'C:\Windows\System32\cmd.exe',"$guest\native-cmd-$mode.txt",'/d /c exit 0')
        if($mode -eq 'no-console'){$operation+='no-console'}
        Guest $operation
        Guest @('copyFileFromGuestToHost',$VMX,"$guest\native-cmd-$mode.txt","$results\native-cmd-$mode.txt")
        $output=[IO.File]::ReadAllText("$results\native-cmd-$mode.txt")
        if($output -notmatch 'CreateProcess=1 Error=0' -or $output -notmatch 'KexDllLoaded=0'){throw 'Native-parent launch reference invalid'}
        if($output -match 'ChildWait=00000000 ChildExitCode=00000000'){$outcome='NaturalExit0'}
        elseif($output -match 'ChildWait=00000102 ChildExitCode=0000dead'){$outcome='TimedOutAndTerminatedOwnedChild'}
        else{throw 'Unexpected child result; inspect raw log'}
        if($mode -eq 'normal' -and $outcome -ne 'NaturalExit0'){throw 'Normal cmd reference failed'}
        $flag=if($mode -eq 'normal'){'00000000'}else{'08000000'}
        if($output -notmatch "CreationFlags=$flag"){throw 'Unexpected creation flags'}
        $receipt+=[pscustomobject]@{
            Architecture=$arch;VMX=$VMX;GuestUser=$GuestUser;Mode=$mode;Outcome=$outcome;Output=$output
            ProbeSHA256=(Get-FileHash $probe).Hash
            SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_native_launch_probe.c").Hash
            RunnerSHA256=(Get-FileHash $PSCommandPath).Hash
            Scope='Native-parent command launch reference; optional CREATE_NO_WINDOW; timeout kills only the owned child; no inference about child KexDll loading or root cause'
        }
    }
}
$receipt|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$root\audit\KxNtParity\launch-creation-flags-receipt.json"
Write-Host 'Launch observations recorded; timeouts are diagnostic outcomes, not functional success'
