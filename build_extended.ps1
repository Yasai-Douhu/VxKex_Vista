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

function Invoke-ClCompile {
    param(
        [string]$SourceFile,
        [string]$OutputFile,
        [string]$DefineFlag,
        [string]$OutputDir
    )
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
    
    $includeFlags = @("/I", "`"$HDR_DIR`"")
    $foFlag = "/Fo" + $OutputFile
    $srcFlag = "`"$SourceFile`""
    $flags = @("/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-")
    $flags += @("/D", "WIN32")
    $flags += @("/D", "NDEBUG")
    $flags += @("/D", "_WINDOWS")
    $flags += @("/D", "_USRDLL")
    $flags += @("/D", $DefineFlag)
    $flags += @("/D", "_WIN32_WINNT=0x0600")
    $flags += @("/D", "WINVER=0x0600")
    $flags += $includeFlags
    $flags += @($foFlag, $srcFlag)
    
    $proc = Start-Process -FilePath "cl.exe" -ArgumentList $flags -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        return $false
    }
    return $true
}

# Build each extended DLL
foreach ($dllName in $ExtendedDLLs) {
    $dllDir = Join-Path $ScriptDirAbs $dllName
    $dllOutDir = Join-Path $ScriptDirAbs "x64\Release\$dllName"
    $exportDef = "${dllName}_EXPORTS"
    
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
            Write-Host " FAILED" -ForegroundColor Red
            break
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
        
        $mlArgs = @("/nologo", "/c", "/Fo`"$asmObjPath`"", "`"$asmFilePath`"")
        Write-Host "  Assembling: $($asmFile.Name)"
        $process = Start-Process -FilePath "ml64.exe" -ArgumentList $mlArgs -NoNewWindow -Wait -PassThru
        if ($process.ExitCode -ne 0) {
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
                  "/SETCHECKSUM", "/LIBPATH:`"$IMPORT_LIBS_DIR`"", "/LIBPATH:`"$ScriptDirAbs\VistaDLLs`"")
    
    if ($defPath) {
        $linkArgs += "/DEF:`"$defPath`""
    }
    
    # Add object files
    foreach ($obj in $objFiles) {
        $linkArgs += "`"$obj`""
    }
    
    # Add dependencies
    $linkArgs += "`"$KexDllLib`"", "`"$KexPathCchLib`"", "`"$KexSmpLib`"", "`"$KexMlsLib`""
    $linkArgs += "`"$ntdllLib`"", "`"$msvcrtLib`""
    $linkArgs += "kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib", "shlwapi.lib"
    
    Write-Host "  Linking $dllName.dll..." -NoNewline
    $proc = Start-Process -FilePath "link.exe" -ArgumentList $linkArgs -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        continue
    }
    Write-Host " OK" -ForegroundColor Green
    
    # Verify output
    if (Test-Path $dllPath) {
        $dllItem = Get-Item $dllPath
        $dllSizeKB = [math]::Round($dllItem.Length / 1KB, 2)
        Write-Host "  $dllName.dll: OK ($dllSizeKB KB)" -ForegroundColor Green
        Copy-Item -Path $dllPath -Destination (Join-Path $ScriptDirAbs "VistaDLLs") -Force
        Copy-Item -Path $libPath -Destination (Join-Path $ScriptDirAbs "VistaDLLs") -Force
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Extended DLLs Build Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

