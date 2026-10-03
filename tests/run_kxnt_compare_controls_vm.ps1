param(
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$results="$root\audit\KxNtParity"
$receipts=@()
function Invoke-Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $guest="$GuestDirectory\$arch"
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword directoryExistsInGuest $VMX $guest | Out-Null
    if($LASTEXITCODE){Invoke-Guest @('createDirectoryInGuest',$VMX,$guest)}
    $prefix=if($arch -eq 'x86'){'Installer\Kex32'}else{'Installer'}
    $hashes=@{}
    foreach($dll in @('KexDll','KxNt')) {
        $source="$root\$prefix\$dll.dll"
        Invoke-Guest @('copyFileFromHostToGuest',$VMX,$source,"$guest\$dll.dll")
        $hashes[$dll]=(Get-FileHash $source).Hash
    }
    $exe="$results\$arch\compare.exe"
    if(!(Test-Path $exe)){throw 'Build probes first: tests/build_kxnt_probes.ps1'}
    Invoke-Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guest\compare.exe")
    foreach($mode in @('NoQuery','LookupOnly','AllocateOnly','SystemOnly','IdleOnly','ConsoleOnly')) {
        & $VMRun -T ws -gu $GuestUser -gp $GuestPassword runProgramInGuest $VMX "$guest\compare.exe" "$guest\KxNt.dll" "$guest\compare-$mode.txt" $mode
        $exitCode=$LASTEXITCODE
        $log="$results\$arch\compare-$mode.txt"
        Invoke-Guest @('copyFileFromGuestToHost',$VMX,"$guest\compare-$mode.txt",$log)
        $output=[IO.File]::ReadAllText($log)
        if($output -notmatch 'ConcurrentCalls=0 HandleDelta='){throw "Incomplete control $arch/$mode"}
        if($mode -eq 'ConsoleOnly' -and ($output -notmatch 'KexDllLoaded=0' -or $output -notmatch 'CompareCases=0')) {
            throw "Console-only control accidentally used the compatibility DLL ($arch)"
        }
        $lines=$output -split '\r?\n'
        $initial=@($lines | Where-Object {$_ -match '^Diagnostic initial '} | ForEach-Object {$_ -replace '^Diagnostic initial ',''})
        $before=@($lines | Where-Object {$_ -match '^Diagnostic before '} | ForEach-Object {$_ -replace '^Diagnostic before ',''})
        $after=@($lines | Where-Object {$_ -match '^Diagnostic after '} | ForEach-Object {$_ -replace '^Diagnostic after ',''})
        if(!$initial.Count -or !$before.Count -or !$after.Count){throw "Missing handle snapshots $arch/$mode"}
        $added=@(Compare-Object $initial $before | Where-Object {$_.SideIndicator -eq '=>'} | ForEach-Object {$_.InputObject})
        $measured=@(Compare-Object $before $after | ForEach-Object {"$($_.SideIndicator) $($_.InputObject)"})
        $receipts += [pscustomobject]@{
            Architecture=$arch; Mode=$mode; GuestExitCode=$exitCode;
            AvailableBinarySHA256=$hashes; ProbeSHA256=(Get-FileHash $exe).Hash;
            AddedDuringInitialization=$added; MeasuredSnapshotChanges=$measured; Output=$output
        }
        Write-Host "$arch ${mode}: exit=$exitCode, measured snapshot changes=$($measured.Count)"
    }
}
# Controls are observations, not a substitute for the main native parity gate.
# Record nonzero guest results too: asynchronous console initialization may
# occur during a short control's measured phase and must remain visible.
$receipts | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$results\compare-controls-receipt.json"
