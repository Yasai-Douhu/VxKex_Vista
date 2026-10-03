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
$receipt=@()
function Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $exe="$results\$arch\console-classification.exe"
    if(!(Test-Path $exe)){throw "Build probes first ($arch)"}
    $guest="$GuestDirectory\$arch"
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword directoryExistsInGuest $VMX $guest | Out-Null
    if($LASTEXITCODE){Guest @('createDirectoryInGuest',$VMX,$guest)}
    Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guest\console-classification.exe")
    Guest @('runProgramInGuest',$VMX,"$guest\console-classification.exe","$guest\console-classification.txt")
    $log="$results\$arch\console-classification-vista.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\console-classification.txt",$log)
    $output=[IO.File]::ReadAllText($log)
    # These gates prove the legacy namespace conflict, not an adapter's success.
    $required=@(
        'VerifyExport=1 KexDllLoaded=0',
        'Class=screen-rw .* ModeOk=1 .* FileType=2 .* Verified=1',
        'Class=screen-write-only .* ModeOk=0 .* FileType=2 .* Verified=1',
        'NativeWrite=file-tag3 Status=00000000 IoStatus=00000000 Bytes=1',
        'Class=console-file-numeric-collision .* ObjectType=File .* ModeOk=1 .* FileType=2 .* Verified=1',
        'NativeWrite=console-file-numeric-collision Status=00000000 IoStatus=00000000 Bytes=1',
        'ConsoleWrite=console-file-numeric-collision Ok=1 Bytes=1',
        'ConsoleFileCollisionFound=1',
        'DisposableFileBytes=2 Text=NN',
        'ConsoleWrite=screen-write-only Ok=1 Bytes=1',
        'Result=MEASURED'
    )
    foreach($pattern in $required){if($output -notmatch $pattern){throw "Missing observation ($arch): $pattern`n$output"}}
    if($output -match 'ObjectType=Console'){throw "Unexpected modern console architecture ($arch)"}
    $receipt += [pscustomobject]@{
        Architecture=$arch; VMX=$VMX; ProbeSHA256=(Get-FileHash $exe).Hash;
        Output=$output; Interpretation='Legacy console and NT kernel handle namespaces overlap; no adapter implemented.'
    }
    Write-Host "$arch legacy console classification: observations verified"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\console-classification-receipt.json"
