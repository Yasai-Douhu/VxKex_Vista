[CmdletBinding()]
param(
    [string[]]$Only = @(),
    [string]$OutputRoot = "",
    [switch]$NoStage
)

# VxKex_Vista Extended DLLs Build Script with VS2010 (cl.exe)
# Builds KxAdvapi, KxBase, KxCom, KxCrt, KxCryp, KxDw, KxDx, KxMi, KxNet, KxSChanl, KxUia, KxUser

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VxKex_Vista Extended DLLs Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VS10_BIN = "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64"
$SDK71_BIN = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin"
$SDK71_INCLUDE = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Include"
$SDK71_LIB = "C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64"

$env:PATH = "$VS10_BIN;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;$SDK71_BIN;$env:PATH"
$env:INCLUDE = "$SDK71_INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
$env:LIB = "$SDK71_LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64"

$ScriptDirAbs = (Get-Item $ScriptDir).FullName
if (!$OutputRoot) { $OutputRoot = Join-Path $ScriptDirAbs "x64\Release" }
$HDR_DIR = Join-Path $ScriptDirAbs "00-Common-Headers"
$KEXDLL_OUT = Join-Path $ScriptDirAbs "VistaDLLs"
$KEXPATHCCH_OUT = Join-Path $ScriptDirAbs "VistaDLLs"
$KEXSMP_OUT = Join-Path $ScriptDirAbs "VistaDLLs"
$KEXMLS_OUT = Join-Path $ScriptDirAbs "VistaDLLs"
$IMPORT_LIBS_DIR = Join-Path $ScriptDirAbs "00-Import-Libraries"

$KexDllLib = Join-Path $KEXDLL_OUT "KexDll.lib"
$KexPathCchLib = Join-Path $KEXPATHCCH_OUT "KexPathCch.lib"
$KexSmpLib = Join-Path $KEXSMP_OUT "KexSmp.lib"
$KexMlsLib = Join-Path $KEXMLS_OUT "KexMls.lib"
$ntdllLib = Join-Path $IMPORT_LIBS_DIR "ntdll_x64.lib"
$msvcrtLib = Join-Path $IMPORT_LIBS_DIR "msvcrt_x64.lib"

# Extended DLLs to build
$ExtendedDLLs = @(
    "KxAdvapi", "KxCom", "KxCrt", "KxCryp", "KxDw",
    "KxDx", "KxMi", "KxNet", "KxSChanl", "KxUia", "KxUser"
)
if ($Only.Count -gt 0) {
    foreach ($name in $Only) {
        if ($name -notin $ExtendedDLLs) { throw "Unknown extended DLL: $name" }
    }
    $ExtendedDLLs = @($ExtendedDLLs | Where-Object { $_ -in $Only })
}

function Invoke-ClCompile {
    param(
        [string]$SourceFile,
        [string]$OutputFile,
        [string]$DefineFlag,
        [string]$OutputDir
    )
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
    
    $includeFlags = @("/I", $HDR_DIR)
    $foFlag = "/Fo" + $OutputFile
    $srcFlag = $SourceFile
    $flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-")
    $flags += @("/D", "WIN32")
    $flags += @("/D", "NDEBUG")
    $flags += @("/D", "_WINDOWS")
    $flags += @("/D", "_USRDLL")
    $flags += @("/D", $DefineFlag)
    $flags += @("/D", "UNICODE", "/D", "_UNICODE")
    if ($DefineFlag -eq "KXSCHANL_EXPORTS") {
        # ByteSwap* must be emitted as CPU intrinsics, and the SSPI strings
        # in this project use the wide-character Win32 API variants.
        $flags += @("/Oi")
    }
    $flags += $includeFlags
    $flags += "/Fd$OutputDir\compile.pdb"
    $flags += @($foFlag, $srcFlag)
    
    & cl.exe @flags | Out-Host
    if ($LASTEXITCODE -ne 0) {
        return $false
    }
    return $true
}

# Build each extended DLL
foreach ($dllName in $ExtendedDLLs) {
    $dllDir = Join-Path $ScriptDirAbs $dllName
    $dllOutDir = Join-Path $OutputRoot $dllName
    $exportDef = "$($dllName.ToUpper())_EXPORTS"
    
    if (-not (Test-Path $dllDir)) {
        Write-Host "`n[dllName] Directory not found: $dllDir" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Building $dllName..." -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    # Create output directory
    if (-not (Test-Path $dllOutDir)) { New-Item -ItemType Directory -Path $dllOutDir | Out-Null }
    
    # Compile source files
    $objFiles = @()
    $srcFiles = Get-ChildItem -Path $dllDir -Filter "*.c" | ForEach-Object { $_.Name }
    
    foreach ($src in $srcFiles) {
        $srcPath = Join-Path $dllDir $src
        $objFile = Join-Path $dllOutDir ($src -replace '\.c$', '.obj')
        $objFiles += $objFile
        
        Write-Host "  Compiling: $src" -NoNewline
        if (!(Invoke-ClCompile $srcPath $objFile $exportDef $dllOutDir)) {
            throw "Compilation failed: $srcPath"
        }
        Write-Host " OK" -ForegroundColor Green
    }
    
    # Link DLL
    $dllPath = Join-Path $dllOutDir "$dllName.dll"
    $libPath = Join-Path $dllOutDir "$dllName.lib"
    
    # Find def file
    $defPath = $null
    
    # Compile ASM files if any
    $asmFiles = Get-ChildItem -Path $dllDir -Filter "*.asm"
    foreach ($asmFile in $asmFiles) {
        $asmFilePath = $asmFile.FullName
        $asmObjPath = Join-Path $dllOutDir ($asmFile.Name -replace '\.asm$', '.obj')
        $objFiles += $asmObjPath
        
        $mlArgs = @("/nologo", "/c", "/Fo$asmObjPath", $asmFilePath)
        Write-Host "  Assembling: $($asmFile.Name)"
        & ml64.exe @mlArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Host " FAILED" -ForegroundColor Red
            continue
        }
    }

    $defFiles = Get-ChildItem -Path $dllDir -Filter "*.def"
    if ($defFiles.Count -gt 0) {
        $defPath = Join-Path $dllDir $defFiles[0].Name
    }
    
    $linkArgs = @("/NOLOGO", "/DLL", "/OUT:$dllPath", "/IMPLIB:$libPath",
                  "/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", "/MACHINE:X64", "/ENTRY:DllMain", "/LTCG",
                  "/LIBPATH:$OutputRoot\KxCryp", "/LIBPATH:$IMPORT_LIBS_DIR", "/LIBPATH:$ScriptDirAbs\VistaDLLs")
    
    if ($defPath) {
        $linkArgs += "/DEF:$defPath"
    }
    
    # Add object files
    foreach ($obj in $objFiles) {
        $linkArgs += $obj
    }
    
    # Add dependencies
    $linkArgs += $KexDllLib, $KexPathCchLib, $KexSmpLib, $KexMlsLib
    $linkArgs += $ntdllLib, $msvcrtLib
    $linkArgs += "kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib", "shlwapi.lib"
    
    $resourceSource = Join-Path $dllDir "$dllName.rc"
    if (Test-Path -LiteralPath $resourceSource) {
        $resource = Join-Path $dllOutDir "$dllName.res"
        & rc.exe /nologo /i $HDR_DIR /fo $resource $resourceSource
        if ($LASTEXITCODE) { throw "Resource compilation failed: $dllName" }
        $linkArgs += $resource
    }
    Write-Host "  Linking $dllName.dll..." -NoNewline
    & link.exe @linkArgs
    if ($LASTEXITCODE -ne 0) { throw "Link failed: $dllName" }
    Write-Host " OK" -ForegroundColor Green
    
    # Verify output
    if (Test-Path $dllPath) {
        $dllItem = Get-Item $dllPath
        $dllSizeKB = [math]::Round($dllItem.Length / 1KB, 2)
        Write-Host "  $dllName.dll: OK ($dllSizeKB KB)" -ForegroundColor Green
        if (!$NoStage) {
            Copy-Item -Path $dllPath -Destination (Join-Path $ScriptDirAbs "VistaDLLs") -Force
            Copy-Item -Path $libPath -Destination (Join-Path $ScriptDirAbs "VistaDLLs") -Force
        }
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Extended DLLs Build Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

