# VxKex_Vista KexShlEx Build Script with VS2010 (cl.exe)
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VxKex_Vista KexShlEx Build with VS2010 (cl.exe)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$VS10_BIN32 = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$SDK70A_BIN = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin"
$SDK70A_INCLUDE = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Include"
$SDK70A_LIB = "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib"

$env:PATH = "$VS10_BIN;$VS10_BIN32;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;$SDK70A_BIN;$env:PATH"
$env:INCLUDE = "$SDK70A_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK70A_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$SRC_DIR = Join-Path $ScriptDirAbs "KexShlEx"
$OUT_DIR = Join-Path $ScriptDirAbs "Win32\Release\KexShlEx"

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
    & "cl.exe" @flags
    if ($LASTEXITCODE -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        return $false
    }
    Write-Host " OK" -ForegroundColor Green
    return $true
}

function Invoke-RcCompile {
    param(
        [string]$SourceFile,
        [string]$OutputFile
    )
    $includeFlags = @("/i", "`"$HDR_DIR`"")
    $foFlag = "/fo" + $OutputFile
    $srcFlag = "`"$SourceFile`""
    $flags = $includeFlags + @($foFlag, $srcFlag)
    $srcName = Split-Path $SourceFile -Leaf
    Write-Host "  Compiling RC: " $srcName -NoNewline
    & "rc.exe" @flags
    if ($LASTEXITCODE -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        return $false
    }
    Write-Host " OK" -ForegroundColor Green
    return $true
}

Write-Host "" -ForegroundColor Yellow
Write-Host "[1/3] Compiling KexShlEx resource files..." -ForegroundColor Yellow

$RcSrc = Join-Path $SRC_DIR "KexShlEx.rc"
$RcObj = Join-Path $OUT_DIR "KexShlEx.res"
if (!(Invoke-RcCompile $RcSrc $RcObj)) {
    exit 1
}

Write-Host "" -ForegroundColor Yellow
Write-Host "[2/3] Compiling KexShlEx source files..." -ForegroundColor Yellow

$Sources = @("ckxshlex.c", "clsfctry.c", "dllmain.c", "gui.c", "lnkfile.c")
$ObjFiles = @()
$allOk = $true

foreach ($srcFile in $Sources) {
    $src = Join-Path $SRC_DIR $srcFile
    $objName = $srcFile -replace '\.c$', '.obj'
    $objFile = Join-Path $OUT_DIR $objName
    $ObjFiles += $objFile
    
    $defines = @("/D", "WIN32",  "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "UNICODE", "/D", "_UNICODE")
    
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

$dllPath = Join-Path $OUT_DIR "KexShlEx.dll"
$libPath = Join-Path $OUT_DIR "KexShlEx.lib"
$defPath = Join-Path $SRC_DIR "KexShlEx.def"

Write-Host "" -ForegroundColor Yellow
Write-Host "[3/3] Linking KexShlEx.dll..." -ForegroundColor Yellow

# Use libraries
$kexGuiLib = Join-Path $ScriptDirAbs "Win32\Release\KexGui\KexGui.lib"

$PREBUILT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\Win32\Release"
$kexDllLib = Join-Path $PREBUILT_LIBS_DIR "KexDll\KexDll.lib"
$kexPathCchLib = Join-Path $PREBUILT_LIBS_DIR "KexPathCch\KexPathCch.lib"
$kexSmpLib = Join-Path $PREBUILT_LIBS_DIR "KexSmp\KexSmp.lib"
$kexMlsLib = Join-Path $PREBUILT_LIBS_DIR "KexMLS\KexMls.lib"
$kexCfgHlpLib = Join-Path $ScriptDirAbs "Win32\Release\KxCfgHlp\KxCfgHlp.lib"
$kexW32MlLib = Join-Path $ScriptDirAbs "Win32\Release\KexW32ML\KexW32ML.lib"

$IMPORT_LIBS_DIR = "C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\00-Import-Libraries"
$ntdllLib = Join-Path $IMPORT_LIBS_DIR "ntdll_x86.lib"
$msvcrtLib = Join-Path $IMPORT_LIBS_DIR "msvcrt_x64.lib"
$kernel32Lib = Join-Path $IMPORT_LIBS_DIR "kernel32_x86.lib"
$user32Lib = Join-Path $IMPORT_LIBS_DIR "user32_x86.lib"

$dllOut = "/OUT:" + "`"$dllPath`""
$implibOut = "/IMPLIB:" + "`"$libPath`""
$defOut = "/DEF:" + "`"$defPath`""

$linkArgs = @(
    "/LIBPATH:`"$($ScriptDirAbs)\00-Import-Libraries`"", "/LIBPATH:`"$SDK70A_LIB`"", "/NOLOGO", "/DLL",
    $dllOut, $implibOut,
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF", "/OPT:ICF",
    "/MACHINE:X86",
    "/LTCG",
    $defOut
)

foreach ($obj in $ObjFiles) {
    $linkArgs += "`"$obj`""
}
$linkArgs += "`"$RcObj`""

$linkArgs += @(
    "$kexGuiLib",
    "$ntdllLib",
    "`"$kexDllLib`"",
    "$kexPathCchLib",
    "$kexSmpLib",
    "$kexMlsLib",
    "$kexCfgHlpLib",
    "$kexW32MlLib",
    "advapi32.lib",
    "`"$msvcrtLib`"",
    "`"$kernel32Lib`"",
    "`"$user32Lib`"",
    "shell32.lib",
    "shlwapi.lib",
    "ole32.lib",
    "oleaut32.lib",
    "comctl32.lib",
    "gdi32.lib",
    "comdlg32.lib",
    "uuid.lib"
)

& "link.exe" @linkArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    exit 1
}

Write-Host " OK" -ForegroundColor Green

if (Test-Path $dllPath) {
    $dllItem = Get-Item $dllPath
    $dllSizeKB = [math]::Round($dllItem.Length / 1KB, 2)
    Write-Host "" -ForegroundColor Green
    Write-Host "  KexShlEx.dll: OK ($dllSizeKB KB)" -ForegroundColor Green
}

exit 0






