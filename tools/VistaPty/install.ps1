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

# Auto-detect VS Code resources\app directory if not specified
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
        $check = Join-Path $cand 'node_modules.asar.unpacked\node-pty\build\Release\conpty.node'
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

if (-not (Test-Path $dest)) {
    throw "Target node-pty directory does not exist: $dest"
}

if (-not (Test-Path $target) -and -not (Test-Path $backup)) {
    throw "Neither conpty.node nor backup exists in $dest."
}

foreach ($name in @('conpty.node','winpty.dll','winpty-agent.exe','vista-pty-clear.exe')) {
    $src = Join-Path $PayloadDirectory $name
    if (-not (Test-Path $src)) { throw "Missing payload: $name in $PayloadDirectory" }
}

# Create backup of original conpty.node if not already backed up
if (-not (Test-Path $backup) -and (Test-Path $target)) {
    Write-Host "[*] Backing up original conpty.node to conpty.node.pre-vistapty..."
    Move-Item -LiteralPath $target -Destination $backup -Force
}

Write-Host "[*] Copying WinPTY and VistaPty binaries to $dest..."
foreach ($name in @('winpty.dll','winpty-agent.exe','vista-pty-clear.exe','conpty.node')) {
    $src = Join-Path $PayloadDirectory $name
    $dst = Join-Path $dest $name
    Copy-Item -LiteralPath $src -Destination $dst -Force
}

Write-Host "Successfully installed VistaPty into $AppDirectory"
Write-Host "Restart VS Code to load the integrated terminal."
