# VxKex_Vista - Windows Vista (NT 6.0) x64 Implementation

VxKex_Vista is a Windows Vista (NT 6.0) specific implementation of VxKex, designed for Windows Vista SP2 and Windows Server 2008 SP2.

## Features

- **Target OS**: Windows Vista SP2 / Windows Server 2008 SP2 (NT 6.0) x64
- **Build Toolchain**: Visual Studio 2010 + Windows SDK 7.1A
- **Architecture**: x64 only

## Project Structure

```
VxKex_Vista/
├── 00-Common-Headers/    # Shared header files
├── 00-Import-Libraries/  # Import libraries for linking
├── KxBase/               # KxBase.dll source (kernel32/kernelbase redirection)
├── KxNt/                 # KxNt.dll source (NT API redirection)
├── x64/Release/          # Build output
├── build_all.ps1         # Unified build script
├── build_kxnt.ps1        # KxNt.dll build script
├── build_kxbase.ps1      # KxBase.dll build script
└── README.md
```

## Built DLLs

| DLL | Size | Description |
|-----|------|-------------|
| KxNt.dll | 117 KB | NT API redirection (ntdll.dll functions) |
| KxBase.dll | 147.5 KB | kernel32/kernelbase redirection |
| KexDll.lib | - | Core VxKex functionality (from VxKex-NEXT) |
| KexPathCch.lib | - | Path functions (from VxKex-NEXT) |
| KexSmp.lib | - | Simple utilities (from VxKex-NEXT) |
| KexMls.lib | - | MLS functionality (from VxKex-NEXT) |

## Build Requirements

- Visual Studio 2010 (with C++ tools)
- Windows SDK 7.1A
- PowerShell

## Building

### Build All DLLs

```powershell
powershell -ExecutionPolicy Bypass -File build_all.ps1
```

This will:
1. Copy dependency DLLs from VxKex-NEXT
2. Build KxNt.dll
3. Build KxBase.dll
4. Output all DLLs to `VistaDLLs/` directory

### Build Individual DLLs

```powershell
# Build KxNt.dll only
powershell -ExecutionPolicy Bypass -File build_kxnt.ps1

# Build KxBase.dll only
powershell -ExecutionPolicy Bypass -File build_kxbase.ps1
```

## Vista Compatibility

### kernelbase.dll Handling

Windows Vista (NT 6.0) does not have `kernelbase.dll`. This DLL was introduced in Windows 7 (NT 6.1).

In VxKex_Vista:
- All exports that would forward to `kernelbase.dll` are redirected to `kernel32.dll` or `advapi32.dll`
- The `forwards.c` file contains Vista-compatible export forwarding

### Vista-Compatible APIs

The following APIs are available on Vista:
- All functions in `kernel32.dll` (Vista version)
- All functions in `advapi32.dll` (Vista version)
- NT APIs via `ntdll.dll`

### Vista-Incompatible APIs

The following APIs are NOT available on Vista and require fallback implementations:
- `IsWow64Process2` (Windows 8+)
- `WaitOnAddress` (Windows 8+)
- `InitializeCriticalSectionEx` (Windows 8+)
- SRWLock functions (`AcquireSRWLockExclusive`, etc.) (Windows 7+)

## Testing

### Vista Test Environment

To test on Windows Server 2008 SP2 or Windows Vista SP2:

#### Option 1: Using VMware Shared Folders

1. Build all DLLs using `build_all.ps1`
2. Start the Vista VM in VMware
3. Access the shared folder or use the deployment script:
   ```cmd
   deploy_to_vista.bat
   ```
4. Copy DLLs to the Vista VM via shared folder

#### Option 2: Using USB/Network Transfer

1. Build all DLLs using `build_all.ps1`
2. Copy the entire `VistaDLLs/` folder to USB drive or network share
3. Transfer to Vista VM

### DLL Installation on Vista

Copy the following DLLs to the target application directory or System32:

```cmd
copy KxNt.dll C:\Windows\System32\
copy KxBase.dll C:\Windows\System32\
copy KexDll.dll C:\Windows\System32\
```

Register DLLs if needed:

```cmd
regsvr32 C:\Windows\System32\KxNt.dll
regsvr32 C:\Windows\System32\KxBase.dll
regsvr32 C:\Windows\System32\KexDll.dll
```

### Verification

Verify the DLLs are working correctly:
1. Check that `KxNt.dll` and `KxBase.dll` load without errors
2. Verify that kernel32.dll function redirection works
3. Test NT API calls through KxNt.dll
4. Use Dependency Walker to verify export resolution

## Known Issues

1. Some Windows 7+ APIs are not available on Vista and may cause loading errors
2. SRWLock functions require Vista SP1 or later
3. `InitializeCriticalSectionEx` requires Windows 8+ fallback implementation

## Related Projects

- [VxKex-NEXT](https://github.com/vxiiduu/VxKex) - Main VxKex project (Windows 7+)
- [VISTA_PORT_ANALYSIS.md](VxKex-NEXT/VISTA_PORT_ANALYSIS.md) - Vista port analysis
- [VISTA_DLL_DIFFERENTIAL_ANALYSIS.md](VxKex-NEXT/VISTA_DLL_DIFFERENTIAL_ANALYSIS.md) - Vista/Windows 7 DLL differences

## License

See the main VxKex project for license information.
