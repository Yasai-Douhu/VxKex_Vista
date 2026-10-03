param(
    [Parameter(Mandatory=$true)][string]$ZigExecutable,
    [Parameter(Mandatory=$true)][string]$VMX,
    [Parameter(Mandatory=$true)][string]$GuestPassword,
    [string]$GuestUser='Administrator',
    [string]$GuestDirectory='C:\KxNtParity',
    [string]$VMRun='C:\Program Files\VMware\VMware Workstation\vmrun.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$results="$root\audit\KxNtParity"
$version=(& $ZigExecutable version | Out-String).Trim()
if($LASTEXITCODE -or $version -ne '0.16.0'){throw 'Reference pins Zig 0.16.0; production compatibility must not pin versions.'}
$receipt=@()
function Guest([string[]]$Arguments) {
    & $VMRun -T ws -gu $GuestUser -gp $GuestPassword @Arguments
    if($LASTEXITCODE){throw "VMware operation failed: $($Arguments[0])"}
}
foreach($arch in @('x86','x64')) {
    $target=if($arch -eq 'x86'){'x86-windows.vista'}else{'x86_64-windows.vista'}
    $exe="$results\$arch\zig-console-smoke-vista.exe"
    & $ZigExecutable build-exe "$PSScriptRoot\kxnt_zig_console_smoke.zig" -target $target -O ReleaseSafe "-femit-bin=$exe"
    if($LASTEXITCODE){throw "Zig build failed ($arch)"}
    $launch="$results\$arch\native-launch.exe"
    if(!(Test-Path $launch)){throw "Build probes first ($arch)"}
    $guest="$GuestDirectory\$arch"
    foreach($path in @($exe,$launch)) {Guest @('copyFileFromHostToGuest',$VMX,$path,"$guest\$(Split-Path -Leaf $path)")}
    Guest @('runProgramInGuest',$VMX,"$guest\native-launch.exe","$guest\zig-console-smoke-vista.exe","$guest\zig-native-launch.txt")
    $log="$results\$arch\zig-native-launch.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\zig-native-launch.txt",$log)
    $output=[IO.File]::ReadAllText($log)
    foreach($pattern in @('KexDllLoaded=0','CreateProcess=1 Error=0','ChildWait=00000000 ChildExitCode=c0000139','Result=MEASURED')) {
        if($output -notmatch $pattern){throw "Unexpected native reference ($arch): $pattern`n$output"}
    }
    foreach($name in @('RtlQueryPerformanceCounter','RtlQueryPerformanceFrequency','RtlReportSilentProcessExit')) {
        if($output -notmatch "NativeExport=$name Present=0"){throw "Expected missing prerequisite changed: $name ($arch)"}
    }
    $receipt += [pscustomobject]@{
        Architecture=$arch; Target=$target; VMX=$VMX; CompilerVersion=$version;
        CompilerSHA256=(Get-FileHash $ZigExecutable).Hash;
        SourceSHA256=(Get-FileHash "$PSScriptRoot\kxnt_zig_console_smoke.zig").Hash;
        ProgramSHA256=(Get-FileHash $exe).Hash; LaunchProbeSHA256=(Get-FileHash $launch).Hash;
        Output=$output; Interpretation='Native process fails at load with missing entries; console write not reached.'
    }
    Write-Host "$arch Zig native reference: expected missing-entry failure verified"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\zig-native-reference-receipt.json"
