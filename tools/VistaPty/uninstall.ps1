param(
    [Parameter(Position=0)]
    [Alias('addDirectory','Path','Directory')]
    [string]$AppDirectory,
    [string]$PayloadDirectory
)

$ErrorActionPreference = 'Stop'

if (-not $PayloadDirectory) {
    if ($PSScriptRoot) {
        $PayloadDirectory = $PSScriptRoot
    } else {
        $PayloadDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
    }
}

if (-not $AppDirectory) {
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code\resources\app",
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code Insiders\resources\app",
        "$env:ProgramFiles\Microsoft VS Code\resources\app",
        "$env:ProgramFiles\Microsoft VS Code Insiders\resources\app"
    )
    $baseDirs = @(
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code",
        "$env:LOCALAPPDATA\Programs\Microsoft VS Code Insiders"
    )
    foreach ($base in $baseDirs) {
        if (Test-Path $base) {
            Get-ChildItem -Path $base | Where-Object { $_.PSIsContainer } | ForEach-Object {
                $subApp = Join-Path $_.FullName 'resources\app'
                if (Test-Path $subApp) { $candidates += $subApp }
            }
        }
    }
    foreach ($cand in $candidates) {
        $check = Join-Path $cand 'node_modules.asar.unpacked\node-pty\build\Release\conpty.node.pre-vistapty'
        if (Test-Path $check) {
            $AppDirectory = $cand
            break
        }
    }
}

if (-not $AppDirectory -or -not (Test-Path $AppDirectory)) {
    throw "VS Code resources\app directory was not found. Please specify -AppDirectory <path>."
}

$dest = Join-Path $AppDirectory 'node_modules.asar.unpacked\node-pty\build\Release'
$target = Join-Path $dest 'conpty.node'
$backup = Join-Path $dest 'conpty.node.pre-vistapty'

if (-not (Test-Path $backup)) {
    throw "Backup conpty.node.pre-vistapty does not exist in $dest. Nothing to restore."
}

Write-Host "[*] Restoring original conpty.node from backup..."
if (Test-Path $target) { Remove-Item -LiteralPath $target -Force }
Copy-Item -LiteralPath $backup -Destination $target -Force

Write-Host "[*] Removing VistaPty / WinPTY binaries..."
foreach ($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe')) {
    $f = Join-Path $dest $name
    if (Test-Path $f) { Remove-Item -LiteralPath $f -Force }
}

Write-Host "Successfully restored original node-pty in $AppDirectory"
