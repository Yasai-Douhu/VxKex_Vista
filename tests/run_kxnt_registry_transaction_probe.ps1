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
        if($output -notmatch "ReopenTransaction Mode=$mode Missing=0 Create=00000000 Disposition=2 .*BeforeRead=00000000 AfterRead=c0190003"){throw 'Reopened handle transaction binding not proven'}
    }
    foreach($mode in @(2,3)){
        if($output -notmatch "PostFinish Mode=$mode Missing=0 .*WriteStatus=c0190003"){throw 'Write failure after commit unproven'}
    }
    foreach($mode in @(1,2,3)){
        if($output -notmatch "PostCommitReopen Mode=$mode Missing=0 Create=c0190003 Disposition=0 Read=deadbeef"){throw 'Post-commit reopen refusal not proven'}
    }
    if([regex]::Matches($output,'ReopenTransaction Mode=\d Missing=1 Create=deadbeef').Count -ne 4 -or [regex]::Matches($output,'PostCommitReopen Mode=\d Missing=1 Create=deadbeef').Count -ne 4){throw 'Created transactional leaf used as reopening root'}
    if([regex]::Matches($output,'MissingKeyOpen=2 TargetTimestampSame=1').Count -ne 8){throw 'Missing-key rollback/timestamp checks incomplete'}
    if([regex]::Matches($output,'(?m)^Pinned ').Count -ne 96){throw 'Pinned matrix incomplete'}
    if([regex]::Matches($output,'Pinned .* Missing=1 Open=c0000034 Create=deadbeef').Count -ne 48){throw 'Missing key opened/created by pinned path'}
    if([regex]::Matches($output,'Pinned Restricted=2 .* Missing=0 Open=c0000022 Create=deadbeef').Count -ne 16){throw 'Owner-rights denial not proven'}
    if($output -notmatch 'Restricted=1 DaclPresent=1 AceCount=0' -or $output -notmatch 'Restricted=2 DaclPresent=1 AceCount=1'){throw 'Effective ACLs not verified'}
    foreach($restricted in @(0,1)){
        foreach($mode in @(1,2,3)){
            $read=if($mode -eq 2){'c0000022'}else{'00000000'}
            $write=if($mode -eq 1){'c0000022'}else{'00000000'}
            if($output -notmatch "Pinned Restricted=$restricted PinAccess=00020000 Mode=$mode Missing=0 Open=00000000 Create=00000000 Disposition=2 .*Read=$read Write=$write"){throw 'Durable pinned privilege/access behavior incomplete'}
        }
    }
    if($output -notmatch 'DeleteOwnedTargetStatus=00000000'){throw 'Owned target not deleted'}
    $receipt += [pscustomobject]@{
        Architecture=$arch;VMX=$VMX;ProbeSHA256=(Get-FileHash $probe).Hash
        SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_registry_transaction_probe.c").Hash
        RunnerSHA256=(Get-FileHash $PSCommandPath).Hash
        HostOutput=[IO.File]::ReadAllText("$results\$arch\registry-transaction-host.txt")
        Output=$output;Conclusion='Committed transaction-bound key cannot directly replace the durable native NtOpenKeyEx handle'
        PinnedConclusion='Empty-relative-name create provides durable privileged handles if initial pin succeeds; OWNER RIGHTS denial blocks all tested pin masks even with backup/restore'
        TransactionReopenConclusion='NtCreateKey with an empty name and transaction-bound root inherits binding; reads fail after commit; reopening after commit returns TRANSACTION_NOT_ACTIVE'
        Scope='Owned HKCU fixtures; private token; four privilege modes; three ACL states; four pin masks; missing-key refusal; effective ACL checks; marker read/write; native DELETE handle cleanup'
    }
    Write-Host "${arch}: transaction binding fails; pinned durable handles work conditionally; protected pin denied; owned fixture deleted"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\registry-transaction-receipt.json"
