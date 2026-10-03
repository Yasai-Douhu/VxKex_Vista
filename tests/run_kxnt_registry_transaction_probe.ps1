param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot;$results="$root\audit\KxNtParity";$receipt=@()
function Guest([string[]]$Arguments){
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')){
    $guest="$GuestDirectory\$arch";$probe="$results\$arch\registry-transaction.exe"
    & $probe "$results\$arch\registry-transaction-host.txt"
    if($LASTEXITCODE){throw "Host transaction probe failed ($arch)"}
    Guest @('copyFileFromHostToGuest',$VMX,$probe,"$guest\registry-transaction.exe")
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\registry-transaction.exe" "$guest\registry-transaction.txt"
    $probeExit=$LASTEXITCODE
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\registry-transaction.txt","$results\$arch\registry-transaction-vista.txt")
    $output=[IO.File]::ReadAllText("$results\$arch\registry-transaction-vista.txt")
    if($probeExit -or $output -notmatch 'Failures=0 Result=PASS'){throw "VM fixture/token/transaction safety test failed ($arch)"}
    if($output -match 'PrivilegesUnavailable=1' -or $output -notmatch 'NativeExPresent=0'){throw 'VM reference capabilities unexpected'}
    foreach($mode in @(1,3)){
        if($output -notmatch "Mode=$mode Missing=0 Create=00000000 Disposition=2 .*BeforeQuery=00000000 AfterQuery=c0190003"){throw 'Read before/after commit behavior unproven'}
        if($output -notmatch "PostFinish Mode=$mode Missing=0 BeforeMarker=69133742"){throw 'Original marker not read'}
    }
    foreach($mode in @(2,3)){
        if($output -notmatch "PostFinish Mode=$mode Missing=0 .*WriteStatus=c0190003"){throw 'Write failure after commit unproven'}
    }
    if([regex]::Matches($output,'MissingKeyOpen=2 TargetTimestampSame=1').Count -ne 8){throw 'Missing-key rollback/timestamp checks incomplete'}
    $receipt += [pscustomobject]@{
        Architecture=$arch;VMX=$VMX;ProbeSHA256=(Get-FileHash $probe).Hash
        SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_registry_transaction_probe.c").Hash
        HostOutput=[IO.File]::ReadAllText("$results\$arch\registry-transaction-host.txt")
        Output=$output;Conclusion='Committed transaction-bound key cannot directly replace the durable native NtOpenKeyEx handle'
        Scope='Owned HKCU fixtures; thread-private duplicated token; modes none/backup/restore/both; rollback of created leaves; cleanup verified'
    }
    Write-Host "$arch raw native transaction experiment: commit invalidates privileged key operations; approach rejected"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\registry-transaction-receipt.json"
