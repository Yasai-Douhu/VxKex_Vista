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
    $exe="$results\$arch\silent-exit.exe"
    $hostLog="$results\$arch\silent-exit-native.txt"
    & $exe ntdll.dll $hostLog
    if($LASTEXITCODE){throw "Native silent-exit reference failed ($arch)"}
    $hostOutput=[IO.File]::ReadAllText($hostLog)
    $guest="$GuestDirectory\$arch"
    $platform=if($arch -eq 'x86'){'Win32'}else{'x64'}
    $hashes=@{}
    foreach($dll in @('KexDll','KxNt')) {
        $source="$root\$platform\Release\$dll\$dll.dll"
        $hashes[$dll]=(Get-FileHash $source).Hash
        Guest @('copyFileFromHostToGuest',$VMX,$source,"$guest\$dll.dll")
    }
    Guest @('copyFileFromHostToGuest',$VMX,$exe,"$guest\silent-exit.exe")
    Guest @('runProgramInGuest',$VMX,"$guest\silent-exit.exe","$guest\KxNt.dll","$guest\silent-exit.txt",'Vista')
    $log="$results\$arch\silent-exit.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\silent-exit.txt",$log)
    $output=[IO.File]::ReadAllText($log)
    foreach($pattern in @('RepeatCalls=1000 HandleDelta=0','Failures=0 ReportingSupported=0','Result=PASS')) {
        if($output -notmatch $pattern){throw "Silent-exit rejection checks failed ($arch): $pattern"}
    }
    $actual=@($output -split '\r?\n' | Where-Object {$_ -match '^Case='})
    $expected=@($hostOutput -split '\r?\n' | Where-Object {$_ -match '^Case='} | ForEach-Object {
        if($_ -match '^Case=self(?: |-)'){$_ -replace 'Status=00000000','Status=c00000bb'}else{$_}
    })
    if($actual.Count -ne 9 -or (Compare-Object $expected $actual)){throw "Handle validation differs from native ($arch)"}
    $imports="$results\$arch\import-resolution.exe"
    $program="$results\$arch\zig-console-smoke-vista.exe"
    if(!(Test-Path $program)){throw 'Build Zig reference first'}
    Guest @('copyFileFromHostToGuest',$VMX,$imports,"$guest\import-resolution.exe")
    Guest @('copyFileFromHostToGuest',$VMX,$program,"$guest\zig-console-smoke-vista.exe")
    Guest @('runProgramInGuest',$VMX,"$guest\import-resolution.exe","$guest\zig-console-smoke-vista.exe","$guest\zig-imports.txt","$guest\KxNt.dll")
    $importsLog="$results\$arch\zig-imports.txt"
    Guest @('copyFileFromGuestToHost',$VMX,"$guest\zig-imports.txt",$importsLog)
    $importsOutput=[IO.File]::ReadAllText($importsLog)
    if($importsOutput -notmatch 'Missing=0 Result=RESOLVED' -or $importsOutput -notmatch 'Import=ntdll.dll!RtlReportSilentProcessExit Resolved=1'){throw "Zig imports not resolved ($arch)"}
    $receipt += [pscustomobject]@{
        Architecture=$arch; BinarySHA256=$hashes;ProbeSHA256=(Get-FileHash $exe).Hash;
        HostOutput=$hostOutput;Output=$output;ImportOutput=$importsOutput;
        ImportProbeSHA256=(Get-FileHash $imports).Hash;ProgramSHA256=(Get-FileHash $program).Hash;
        ReportingSupported=$false; Interpretation='Bindability and explicit NOT_SUPPORTED, not WER reporting implementation.'
    }
    Write-Host "$arch explicit silent-exit rejection PASS; Zig imports resolved"
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 "$results\silent-exit-receipt.json"
