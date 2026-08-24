# VxKex_Vista KxNt Build Script with VS2010 (cl.exe)
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VxKex_Vista KxNt Build with VS2010 (cl.exe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$SDK70A_BIN = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin"
$SDK70A_INCLUDE = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Include"
$SDK70A_LIB = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib"

$env:PATH = "$VS10_BIN;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;$SDK70A_BIN;$env:PATH"
$env:INCLUDE = "$SDK70A_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK70A_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib"

Write-Host "" -ForegroundColor Cyan
Write-Host "  Tools:" -ForegroundColor Yellow
Write-Host "  cl.exe: $VS10_BIN\cl.exe" -ForegroundColor Gray
Write-Host "  link.exe: $VS10_BIN\link.exe" -ForegroundColor Gray

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$KXNT_DIR = Join-Path $ScriptDirAbs "KxNt"
$OUT_DIR = Join-Path $ScriptDirAbs "Win32\Release\KxNt"
$IMPORT_LIBS_DIR = Join-Path $ScriptDirAbs "00-Import-Libraries"

# Create all output directories
if (-not (Test-Path $OUT_DIR)) { New-Item -ItemType Directory -Path $OUT_DIR | Out-Null }

function Invoke-ClCompile {
    param(
        [string]$SourceFile,
        [string]$OutputFile,
        [string[]]$Defines,
        [string]$OutputDir
    )
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
    
    $includeFlags = @("/I", "`"$HDR_DIR`"")
    $foFlag = "/Fo" + $OutputFile
    $srcFlag = "`"$SourceFile`""
    $flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-") + $Defines + $includeFlags + @($foFlag, $srcFlag)
    $srcName = Split-Path $SourceFile -Leaf
    Write-Host "  Compiling: " $srcName -NoNewline
    $proc = Start-Process -FilePath "cl.exe" -ArgumentList $flags -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        return $false
    }
    Write-Host " OK" -ForegroundColor Green
    return $true
}

# === Step 1: Compile KxNt source files ===
Write-Host "" -ForegroundColor Yellow
Write-Host "[1/2] Compiling KxNt source files..." -ForegroundColor Yellow

$KxNtSrc = @(
    (Join-Path $KXNT_DIR "dllmain.c"),
    (Join-Path $KXNT_DIR "forwards.c"),
    (Join-Path $KXNT_DIR "stubs.c")
)

$KxNtObj = @()
$allOk = $true

foreach ($src in $KxNtSrc) {
    $srcLeaf = Split-Path $src -Leaf
    $objName = $srcLeaf -replace '\.c$', '.obj'
    $objFile = Join-Path $OUT_DIR $objName
    $KxNtObj += $objFile
    
    $defines = @("/D", "WIN32", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "_USRDLL", "/D", "KXNT_EXPORTS", "/D", "_WIN32_WINNT=0x0600", "/D", "WINVER=0x0600")
    
    if (!(Invoke-ClCompile $src $objFile $defines $OUT_DIR)) {
        $allOk = $false
        break
    }
}

if (-not $allOk) {
    Write-Host "" -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}
$dllPath = Join-Path $OUT_DIR "KxNt.dll"
$libPath = Join-Path $OUT_DIR "KxNt.lib"
$defPath = Join-Path $KXNT_DIR "KxNt.def"

# Import libraries (from VxKex_Vista\00-Import-Libraries)
$IMPORT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\00-Import-Libraries"
$ntdllLib = Join-Path $IMPORT_LIBS_DIR "ntdll_x86.lib"
$msvcrtLib = Join-Path $IMPORT_LIBS_DIR "msvcrt_x64.lib"
$kernel32Lib = Join-Path $IMPORT_LIBS_DIR "kernel32_x86.lib"
$user32Lib = Join-Path $IMPORT_LIBS_DIR "user32_x86.lib"

# Prebuilt libraries (from VxKex_Vista\Win32\Release)
$PREBUILT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\Win32\Release"
$kexDllLib = Join-Path $PREBUILT_LIBS_DIR "KexDll\KexDll.lib"
$kexPathCchLib = Join-Path $PREBUILT_LIBS_DIR "KexPathCch\KexPathCch.lib"
$kexSmpLib = Join-Path $PREBUILT_LIBS_DIR "KexSmp\KexSmp.lib"
$kexMlsLib = Join-Path $PREBUILT_LIBS_DIR "KexMLS\KexMls.lib"
$dllOut = "/OUT:" + "`"$dllPath`""
$implibOut = "/IMPLIB:" + "`"$libPath`""
$defOut = "/DEF:" + "`"$defPath`""

$linkArgs = @(
    "/LIBPATH:`"$($ScriptDirAbs)\00-Import-Libraries`"", "/LIBPATH:`"$SDK70A_LIB`"", "/NOLOGO", "/DLL",
    $dllOut, $implibOut,
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF", "/OPT:ICF",
    "/MACHINE:X86",
    "/ENTRY:DllMain",
    "/LTCG",
    $defOut
)

# Add object files
foreach ($obj in $KxNtObj) {
    $linkArgs += "`"$obj`""
}

# Add libraries
$linkArgs += @(
    "`"$kexDllLib`"",
    "`"$kexPathCchLib`"",
    "`"$kexSmpLib`"",
    "`"$kexMlsLib`"",
    "`"$ntdllLib`"",
    "`"$msvcrtLib`"",
    "`"$kernel32Lib`"",
    "`"$user32Lib`"",
    "advapi32.lib",
    "shlwapi.lib"
)

Write-Host "  Linking KxNt.dll..." -NoNewline
$proc = Start-Process -FilePath "link.exe" -ArgumentList $linkArgs -NoNewWindow -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    Write-Host "" -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}

Write-Host " OK" -ForegroundColor Green

# Verify output
if (Test-Path $dllPath) {
    $dllItem = Get-Item $dllPath
    $dllSizeKB = [math]::Round($dllItem.Length / 1KB, 2)
    Write-Host "" -ForegroundColor Green
    Write-Host "  KxNt.dll: OK ($dllSizeKB KB)" -ForegroundColor Green
}

Write-Host ""
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "BUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

exit 0
