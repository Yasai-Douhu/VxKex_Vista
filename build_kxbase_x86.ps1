# VxKex_Vista KxBase Build Script with VS2010 (cl.exe)
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VxKex_Vista KxBase Build with VS2010 (cl.exe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$SDK71_BIN = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin"
$SDK71_INCLUDE = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include"
$SDK71_LIB = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib"

$env:PATH = "$VS10_BIN;$SDK71_BIN;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib"

Write-Host "" -ForegroundColor Cyan
Write-Host "  Tools:" -ForegroundColor Yellow
Write-Host "  cl.exe: $VS10_BIN\cl.exe" -ForegroundColor Gray
Write-Host "  link.exe: $VS10_BIN\link.exe" -ForegroundColor Gray

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$KXBASE_DIR = Join-Path $ScriptDirAbs "KxBase"
$OUT_DIR = Join-Path $ScriptDirAbs "Win32\Release\KxBase"

# Import libraries (from VxKex-NEXT\00-Import Libraries)
$IMPORT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex-NEXT\00-Import Libraries"
$ntdllLib = Join-Path $IMPORT_LIBS_DIR "ntdll_x86.lib"
$msvcrtLib = Join-Path $IMPORT_LIBS_DIR "msvcrt_x64.lib"
$kernel32Lib = Join-Path $IMPORT_LIBS_DIR "kernel32_x86.lib"
$user32Lib = Join-Path $IMPORT_LIBS_DIR "user32_x86.lib"

# Prebuilt libraries (from VxKex_Vista\Win32\Release)
$PREBUILT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex_Vista\Win32\Release"
$kexDllLib = Join-Path $PREBUILT_LIBS_DIR "KexDll\KexDll.lib"
$kexPathCchLib = Join-Path $PREBUILT_LIBS_DIR "KexPathCch\KexPathCch.lib"
$kexSmpLib = Join-Path $PREBUILT_LIBS_DIR "KexSmp\KexSmp.lib"
$kexMlsLib = Join-Path $PREBUILT_LIBS_DIR "KexMLS\KexMls.lib"

# Create output directory
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

# === Step 1: Compile KxBase source files ===
Write-Host "" -ForegroundColor Yellow
Write-Host "[1/2] Compiling KxBase source files..." -ForegroundColor Yellow

$kxBaseSrc = @(
    (Join-Path $KXBASE_DIR "dllmain.c"),
    (Join-Path $KXBASE_DIR "forwards.c"),
    (Join-Path $KXBASE_DIR "appmodel.c"),
    (Join-Path $KXBASE_DIR "appstate.c"),
    (Join-Path $KXBASE_DIR "cfgmgr.c"),
    (Join-Path $KXBASE_DIR "conansi.c"),
    (Join-Path $KXBASE_DIR "console.c"),
    (Join-Path $KXBASE_DIR "consup.c"),
    (Join-Path $KXBASE_DIR "critsect.c"),
    (Join-Path $KXBASE_DIR "file.c"),
    (Join-Path $KXBASE_DIR "geoname.c"),
    (Join-Path $KXBASE_DIR "handle.c"),
    (Join-Path $KXBASE_DIR "heap.c"),
    (Join-Path $KXBASE_DIR "misc.c"),
    (Join-Path $KXBASE_DIR "module.c"),
    (Join-Path $KXBASE_DIR "process.c"),
    (Join-Path $KXBASE_DIR "pssapi.c"),
    (Join-Path $KXBASE_DIR "security.c"),
    (Join-Path $KXBASE_DIR "stubs.c"),
    (Join-Path $KXBASE_DIR "support.c"),
    (Join-Path $KXBASE_DIR "synch.c"),
    (Join-Path $KXBASE_DIR "thread.c"),
    (Join-Path $KXBASE_DIR "time.c"),
    (Join-Path $KXBASE_DIR "token.c"),
    (Join-Path $KXBASE_DIR "verspoof.c"),
    (Join-Path $KXBASE_DIR "vmem.c"),
    (Join-Path $KXBASE_DIR "wow64.c")
)
$kxBaseObj = @()
$allOk = $true
$kxBaseDefines = @("/D", "WIN32", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "_USRDLL", "/D", "KXBASE_EXPORTS", "/D", "_WIN32_WINNT=0x0600", "/D", "WINVER=0x0600", "/D", "PSAPI_VERSION=1")

foreach ($src in $kxBaseSrc) {
    $srcLeaf = Split-Path $src -Leaf
    $objName = $srcLeaf -replace '\.c$', '.obj'
    $objFile = Join-Path $OUT_DIR $objName
    $kxBaseObj += $objFile
    
    if (!(Invoke-ClCompile $src $objFile $kxBaseDefines $OUT_DIR)) {
        $allOk = $false
        break
    }
}

if (-not $allOk) {
    Write-Host "" -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}

# === Step 2: Link KxBase.dll ===
Write-Host "" -ForegroundColor Yellow
Write-Host "[2/2] Linking KxBase.dll..." -ForegroundColor Yellow

$dllPath = Join-Path $OUT_DIR "KxBase.dll"
$libPath = Join-Path $OUT_DIR "KxBase.lib"
$defPath = Join-Path $KXBASE_DIR "kxbase.def"

$dllOut = "/OUT:" + "`"$dllPath`""
$implibOut = "/IMPLIB:" + "`"$libPath`""
$defOut = "/DEF:" + "`"$defPath`""

$linkArgs = @(
    "/LIBPATH:`"$($ScriptDirAbs)\00-Import-Libraries`"", "/LIBPATH:`"$SDK71_LIB`"", "/NOLOGO", "/DLL",
    $dllOut, $implibOut,
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF", "/OPT:ICF",
    "/MACHINE:X86",
    "/ENTRY:DllMain",
    "/LTCG",
    $defOut,
    "/LIBPATH:`"$IMPORT_LIBS_DIR`"",
    "/LIBPATH:`"$ScriptDirAbs\Win32DLLs`"",
    "KexW32ML.lib"
)

# Add object files
foreach ($obj in $kxBaseObj) {
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
    "shlwapi.lib",
    "psapi.lib",
    "version.lib"
)

Write-Host "  Linking KxBase.dll..." -NoNewline
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
    Write-Host "  KxBase.dll: OK ($dllSizeKB KB)" -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "BUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

exit 0
