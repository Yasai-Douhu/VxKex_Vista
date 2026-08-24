$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64"
$VS10_BIN32 = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin"
$SDK71_BIN = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin"
$SDK71_INCLUDE = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Include"
$SDK71_LIB = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64"

$env:PATH = "$VS10_BIN;$VS10_BIN32;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;$SDK71_BIN;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include;C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\00-Common-Headers"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$SRC_DIR = Join-Path $ScriptDirAbs "KexDll"
$OUT_DIR = Join-Path $ScriptDirAbs "x64\Release\KexDll"

if (-not (Test-Path $OUT_DIR)) { New-Item -ItemType Directory -Path $OUT_DIR | Out-Null }

$cFiles = Get-ChildItem -Path $SRC_DIR -Filter "*.c" | Select-Object -ExpandProperty FullName
$objFiles = @()

$defines = @("/D", "WIN32", "/D", "_WIN64", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "_USRDLL", "/D", "KEXDLL_EXPORTS", "/D", "UNICODE", "/D", "_UNICODE")
$includeFlags = @("/I", "`"$HDR_DIR`"")

foreach ($src in $cFiles) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $objFile = Join-Path $OUT_DIR "$baseName.obj"
    $objFiles += "`"$objFile`""
    
    $flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-") + $defines + $includeFlags + @("/Fo$objFile", "`"$src`"")
    # Write-Host "Compiling $baseName.c..."
    & "cl.exe" @flags
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# Compile ASM file
$asmFile = Join-Path $SRC_DIR "syscal64.asm"
$asmObjFile = Join-Path $OUT_DIR "syscal64.obj"
$objFiles += "`"$asmObjFile`""
Write-Host "Compiling syscal64.asm..."
& "ml64.exe" /c /Fo $asmObjFile $asmFile
if ($LASTEXITCODE -ne 0) { exit 1 }

# Compile Resource
$rcFile = Join-Path $SRC_DIR "KexDll.rc"
$resFile = Join-Path $OUT_DIR "KexDll.res"
Write-Host "Compiling KexDll.rc..."
& "rc.exe" /I "$HDR_DIR" /fo $resFile $rcFile
if ($LASTEXITCODE -ne 0) { exit 1 }

$dllPath = Join-Path $OUT_DIR "KexDll.dll"
$libPath = Join-Path $OUT_DIR "KexDll.lib"
$defFile = Join-Path $SRC_DIR "KexDll.def"
$importLibsDir = Join-Path $ScriptDirAbs "00-Import-Libraries"
$kexSmpLib = Join-Path $ScriptDirAbs "x64\Release\KexSmp\KexSmp.lib"
$kexPathCchLib = Join-Path $ScriptDirAbs "02-Prebuilt DLLs\x64\Release\KexPathCch\KexPathCch.lib"

if (-not (Test-Path $kexPathCchLib)) {
    $kexPathCchLib = Join-Path $ScriptDirAbs "VxKex_Vista\x64\Release\KexPathCch\KexPathCch.lib"
}

$libArgs = @("/NOLOGO", "/DLL", "/INCREMENTAL:NO", "/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", "/DEBUG", "/RELEASE", "/MACHINE:X64", "/LTCG")
$libArgs += @("/OUT:`"$dllPath`"", "/IMPLIB:`"$libPath`"", "/DEF:`"$defFile`"")
$libArgs += @("/LIBPATH:`"$importLibsDir`"", "/LIBPATH:`"$SDK71_LIB`"")
$libArgs += @("ntdll_x64.lib", "kernel32_x64.lib", "user32_x64.lib", "gdi32.lib", "shlwapi.lib")
$libArgs += @("`"$kexSmpLib`"", "KexPathCch.lib")
$libArgs += $objFiles
$libArgs += @("`"$resFile`"")

Write-Host "Linking KexDll.dll..."
& "link.exe" @libArgs
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "KexDll.dll built successfully!" -ForegroundColor Green

