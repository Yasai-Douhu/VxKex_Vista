#include "buildcfg.h"
#include <KxCfgHlp.h>

// Native Vista/Server 2008 TxF operations share a KTM transaction with IFEO.
// These helpers never commit, roll back, or fall back to nontransactional I/O.
// Callers must authorize the roots and roll back the entire operation on error.
static LONG CheckSetupPath(PCWSTR Path, PWSTR FullPath, BOOL MissingAllowed,
    PDWORD Attributes, HANDLE Transaction)
{
    WCHAR Prefix[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA Data;
    DWORD Length, Index, Error;
    WCHAR Saved;
    if (!Path || !Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    if (wcslen(Path) < 3) return ERROR_INVALID_NAME;
    // No relative, UNC, device, alternate-stream, or volume-root operations.
    if (!((Path[0] >= L'A' && Path[0] <= L'Z') || (Path[0] >= L'a' && Path[0] <= L'z')) ||
        Path[1] != L':' || Path[2] != L'\\' || wcschr(Path + 2, L':')) return ERROR_INVALID_NAME;
    Length = GetFullPathName(Path, MAX_PATH, FullPath, NULL);
    if (!Length) return GetLastError();
    if (Length >= MAX_PATH) return ERROR_FILENAME_EXCED_RANGE;
    while (Length > 3 && FullPath[Length - 1] == L'\\') FullPath[--Length] = 0;
    if (Length <= 3) return ERROR_INVALID_NAME;
    StringCchCopy(Prefix, ARRAYSIZE(Prefix), FullPath);
    *Attributes = INVALID_FILE_ATTRIBUTES;
    // Check every existing ancestor, not only the leaf, to refuse junctions.
    for (Index = 3; Index <= Length; ++Index) {
        if (Prefix[Index] && Prefix[Index] != L'\\') continue;
        Saved = Prefix[Index]; Prefix[Index] = 0;
        if (!GetFileAttributesTransacted(Prefix, GetFileExInfoStandard, &Data, Transaction)) {
            Error = GetLastError();
            Prefix[Index] = Saved;
            if (MissingAllowed && (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND)) return 0;
            return Error;
        }
        Prefix[Index] = Saved;
        if (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_INVALID_DATA;
        if (Saved && !(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
        if (!Saved) *Attributes = Data.dwFileAttributes;
    }
    return 0;
}

static BOOL PathContains(PCWSTR Parent, PCWSTR Child)
{
    SIZE_T Length = wcslen(Parent);
    return !_wcsnicmp(Parent, Child, Length) && (!Child[Length] || Child[Length] == L'\\');
}

LONG KxCfgpCopySetupFile(PCWSTR Source, PCWSTR Destination, HANDLE Transaction)
{
    WCHAR SourcePath[MAX_PATH], DestinationPath[MAX_PATH];
    DWORD Attributes;
    LONG Error = CheckSetupPath(Source, SourcePath, FALSE, &Attributes, Transaction);
    if (Error) return Error;
    if (Attributes & FILE_ATTRIBUTE_DIRECTORY) return ERROR_DIRECTORY;
    Error = CheckSetupPath(Destination, DestinationPath, TRUE, &Attributes, Transaction);
    if (Error) return Error;
    if (Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    if (!_wcsicmp(SourcePath, DestinationPath)) return ERROR_INVALID_PARAMETER;
    if (Attributes != INVALID_FILE_ATTRIBUTES) {
        BY_HANDLE_FILE_INFORMATION Information;
        HANDLE File = CreateFileTransacted(DestinationPath, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
        if (File == INVALID_HANDLE_VALUE) return GetLastError();
        if (!GetFileInformationByHandle(File, &Information)) Error = GetLastError();
        else if (Information.nNumberOfLinks > 1) Error = ERROR_INVALID_DATA;
        CloseHandle(File);
        if (Error) return Error;
    }
    return CopyFileTransacted(SourcePath, DestinationPath, NULL, NULL, NULL, 0, Transaction) ? 0 : GetLastError();
}

LONG KxCfgpDeleteSetupFile(PCWSTR Path, HANDLE Transaction)
{
    WCHAR FullPath[MAX_PATH];
    DWORD Attributes;
    LONG Error = CheckSetupPath(Path, FullPath, TRUE, &Attributes, Transaction);
    if (Error) return Error;
    if (Attributes == INVALID_FILE_ATTRIBUTES) return 0;
    if (Attributes & FILE_ATTRIBUTE_DIRECTORY) return ERROR_DIRECTORY;
    return DeleteFileTransacted(FullPath, Transaction) ? 0 : GetLastError();
}

static LONG ProcessSetupTree(PCWSTR Source, PCWSTR Destination, HANDLE Transaction, unsigned Depth)
{
    WCHAR SourcePath[MAX_PATH], TargetPath[MAX_PATH], Search[MAX_PATH];
    WCHAR ChildSource[MAX_PATH], ChildTarget[MAX_PATH];
    DWORD Attributes, EnumerationError;
    LONG Error;
    WIN32_FIND_DATA Data;
    HANDLE Find = INVALID_HANDLE_VALUE;
    if (Depth > 32) return ERROR_INVALID_DATA;
    Error = CheckSetupPath(Source, SourcePath, Destination == NULL, &Attributes, Transaction);
    if (Error) return Error;
    if (Attributes == INVALID_FILE_ATTRIBUTES) return 0;
    if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    if (Destination) {
        Error = CheckSetupPath(Destination, TargetPath, TRUE, &Attributes, Transaction);
        if (Error) return Error;
        if (PathContains(SourcePath, TargetPath) || PathContains(TargetPath, SourcePath)) return ERROR_INVALID_PARAMETER;
        if (Attributes == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryTransacted(NULL, TargetPath, NULL, Transaction)) return GetLastError();
        } else if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    }
    if (FAILED(StringCchPrintf(Search, ARRAYSIZE(Search), L"%s\\*", SourcePath))) return ERROR_FILENAME_EXCED_RANGE;
    Find = FindFirstFileTransacted(Search, FindExInfoStandard, &Data,
        FindExSearchNameMatch, NULL, 0, Transaction);
    if (Find == INVALID_HANDLE_VALUE) {
        Error = GetLastError();
        if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
    } else {
        do {
            if (!wcscmp(Data.cFileName, L".") || !wcscmp(Data.cFileName, L"..")) continue;
            if (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { Error = ERROR_INVALID_DATA; break; }
            if (FAILED(StringCchPrintf(ChildSource, ARRAYSIZE(ChildSource), L"%s\\%s", SourcePath, Data.cFileName)) ||
                (Destination && FAILED(StringCchPrintf(ChildTarget, ARRAYSIZE(ChildTarget), L"%s\\%s", TargetPath, Data.cFileName)))) {
                Error = ERROR_FILENAME_EXCED_RANGE; break;
            }
            if (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                Error = ProcessSetupTree(ChildSource, Destination ? ChildTarget : NULL, Transaction, Depth + 1);
            else Error = Destination ? KxCfgpCopySetupFile(ChildSource, ChildTarget, Transaction) :
                KxCfgpDeleteSetupFile(ChildSource, Transaction);
            if (Error) break;
        } while (FindNextFile(Find, &Data));
        EnumerationError = GetLastError();
        FindClose(Find);
        if (!Error && EnumerationError != ERROR_NO_MORE_FILES) Error = EnumerationError;
    }
    if (!Error && !Destination && !RemoveDirectoryTransacted(SourcePath, Transaction)) Error = GetLastError();
    return Error;
}

LONG KxCfgpCopySetupDirectory(PCWSTR Source, PCWSTR Destination, HANDLE Transaction)
{
    if (!Destination) return ERROR_INVALID_PARAMETER;
    return ProcessSetupTree(Source, Destination, Transaction, 0);
}

LONG KxCfgpRemoveSetupDirectory(PCWSTR Path, HANDLE Transaction)
{
    return ProcessSetupTree(Path, NULL, Transaction, 0);
}

static PCWSTR SetupLibraries[] = { L"KexDll.dll", L"KxBase.dll", L"KxNt.dll",
    L"KxAdvapi.dll", L"KxCom.dll", L"KxCrt.dll", L"KxCryp.dll", L"KxDw.dll",
    L"KxDx.dll", L"KxMi.dll", L"KxNet.dll", L"KxUia.dll", L"KxUser.dll" };

static LONG SetupChildPath(PWSTR Buffer, PCWSTR Root, PCWSTR Child)
{
    return FAILED(StringCchPrintf(Buffer, MAX_PATH, L"%s\\%s", Root, Child)) ?
        ERROR_FILENAME_EXCED_RANGE : 0;
}

static LONG ValidateSetupRoots(PCWSTR *Paths, PWSTR *FullPaths, unsigned Count,
    HANDLE Transaction)
{
    unsigned Index, Other; DWORD Attributes; LONG Error;
    for (Index = 0; Index < Count; ++Index) {
        // Only Target (index 0) may be absent. The package and system roots
        // must already exist; never create an accidentally misspelled system root.
        Error = CheckSetupPath(Paths[Index], FullPaths[Index], Index == 0, &Attributes, Transaction);
        if (Error) return Error;
        if (Attributes != INVALID_FILE_ATTRIBUTES && !(Attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
        for (Other = 0; Other < Index; ++Other)
            if (PathContains(FullPaths[Index], FullPaths[Other]) ||
                PathContains(FullPaths[Other], FullPaths[Index])) return ERROR_INVALID_PARAMETER;
    }
    return 0;
}

static LONG CheckPackageFile(PCWSTR Path, HANDLE Transaction, BOOL Optional)
{
    WCHAR FullPath[MAX_PATH]; DWORD Attributes;
    LONG Error = CheckSetupPath(Path, FullPath, Optional, &Attributes, Transaction);
    if (Error) return Error;
    if (Attributes == INVALID_FILE_ATTRIBUTES) return Optional ? ERROR_FILE_NOT_FOUND : ERROR_INVALID_DATA;
    return (Attributes & FILE_ATTRIBUTE_DIRECTORY) ? ERROR_DIRECTORY : 0;
}

static LONG ReadBinaryPart(HANDLE File, ULONGLONG Offset, PVOID Buffer, DWORD Bytes)
{
    LARGE_INTEGER Position; DWORD Read;
    Position.QuadPart = Offset;
    if (!SetFilePointerEx(File, Position, NULL, FILE_BEGIN)) return GetLastError();
    if (!ReadFile(File, Buffer, Bytes, &Read, NULL)) return GetLastError();
    return Read == Bytes ? 0 : ERROR_BAD_EXE_FORMAT;
}

// Inspect bytes without loading or executing package code. This checks the
// architecture and bounded PE layout, not imports, signatures, or runtime ABI.
LONG KxCfgpValidateSetupBinary(PCWSTR Path, WORD Machine, BOOLEAN Dll, HANDLE Transaction)
{
    HANDLE File; LONG Error; LARGE_INTEGER Size;
    IMAGE_DOS_HEADER Dos; DWORD Signature, ImageSize, HeadersSize, Entry;
    IMAGE_FILE_HEADER Header; IMAGE_SECTION_HEADER Section;
    union {IMAGE_OPTIONAL_HEADER32 Bits32; IMAGE_OPTIONAL_HEADER64 Bits64;} Optional;
    ULONGLONG Offset, Sections; unsigned Index;
    if (Machine != IMAGE_FILE_MACHINE_I386 && Machine != IMAGE_FILE_MACHINE_AMD64) return ERROR_INVALID_PARAMETER;
    Error = CheckPackageFile(Path, Transaction, FALSE);
    if (Error) return Error;
    File = CreateFileTransacted(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
    if (File == INVALID_HANDLE_VALUE) return GetLastError();
    if (!GetFileSizeEx(File, &Size)) { Error = GetLastError(); goto Done; }
    Error = ReadBinaryPart(File, 0, &Dos, sizeof(Dos)); if (Error) goto Done;
    Error = ERROR_BAD_EXE_FORMAT;
    if (Dos.e_magic != IMAGE_DOS_SIGNATURE || Dos.e_lfanew < (LONG)sizeof(Dos)) goto Done;
    Offset = (ULONG)Dos.e_lfanew;
    if (Offset + sizeof(Signature) + sizeof(Header) > (ULONGLONG)Size.QuadPart) goto Done;
    Error = ReadBinaryPart(File, Offset, &Signature, sizeof(Signature)); if (Error) goto Done;
    Error = ReadBinaryPart(File, Offset + sizeof(Signature), &Header, sizeof(Header)); if (Error) goto Done;
    Error = ERROR_BAD_EXE_FORMAT;
    if (Signature != IMAGE_NT_SIGNATURE || Header.Machine != Machine ||
        !(Header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) ||
        !!(Header.Characteristics & IMAGE_FILE_DLL) != !!Dll || !Header.NumberOfSections ||
        Header.NumberOfSections > 96 || Header.SizeOfOptionalHeader !=
        (Machine == IMAGE_FILE_MACHINE_I386 ? sizeof(IMAGE_OPTIONAL_HEADER32) : sizeof(IMAGE_OPTIONAL_HEADER64))) goto Done;
    Offset += sizeof(Signature) + sizeof(Header);
    Sections = Offset + Header.SizeOfOptionalHeader;
    if (Sections + Header.NumberOfSections * sizeof(Section) > (ULONGLONG)Size.QuadPart) goto Done;
    ZeroMemory(&Optional, sizeof(Optional));
    Error = ReadBinaryPart(File, Offset, &Optional, Header.SizeOfOptionalHeader); if (Error) goto Done;
    Error = ERROR_BAD_EXE_FORMAT;
    if (Machine == IMAGE_FILE_MACHINE_I386) {
        if (Optional.Bits32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || Optional.Bits32.NumberOfRvaAndSizes > IMAGE_NUMBEROF_DIRECTORY_ENTRIES) goto Done;
        ImageSize = Optional.Bits32.SizeOfImage; HeadersSize = Optional.Bits32.SizeOfHeaders; Entry = Optional.Bits32.AddressOfEntryPoint;
    } else {
        if (Optional.Bits64.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || Optional.Bits64.NumberOfRvaAndSizes > IMAGE_NUMBEROF_DIRECTORY_ENTRIES) goto Done;
        ImageSize = Optional.Bits64.SizeOfImage; HeadersSize = Optional.Bits64.SizeOfHeaders; Entry = Optional.Bits64.AddressOfEntryPoint;
    }
    if (!ImageSize || HeadersSize > (ULONGLONG)Size.QuadPart ||
        HeadersSize < Sections + Header.NumberOfSections * sizeof(Section) || HeadersSize > ImageSize || Entry >= ImageSize) goto Done;
    for (Index = 0; Index < Header.NumberOfSections; ++Index) {
        Error = ReadBinaryPart(File, Sections + Index * sizeof(Section), &Section, sizeof(Section)); if (Error) goto Done;
        Error = ERROR_BAD_EXE_FORMAT;
        if (Section.SizeOfRawData && (Section.PointerToRawData < HeadersSize ||
            (ULONGLONG)Section.PointerToRawData + Section.SizeOfRawData > (ULONGLONG)Size.QuadPart)) goto Done;
        if ((ULONGLONG)Section.VirtualAddress + max(Section.Misc.VirtualSize, Section.SizeOfRawData) > ImageSize) goto Done;
    }
    Error = 0;
Done:
    CloseHandle(File); return Error;
}

LONG KxCfgpValidateSetupPackage(PCWSTR Package, HANDLE Transaction)
{
    WCHAR PackagePath[MAX_PATH], Path[MAX_PATH]; DWORD Attributes; unsigned View, Index;
    PCWSTR Dlls[] = {L"KexShlEx.dll", L"Kex64\\dwrw10.dll"};
    PCWSTR Exes[] = {L"VistaRun.exe", L"KexCfg.exe", L"VxlView.exe"};
    BOOL Tls[2]; LONG Error = CheckSetupPath(Package, PackagePath, FALSE, &Attributes, Transaction);
    if (Error) return Error;
    if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    for (Index = 0; Index < ARRAYSIZE(Dlls); ++Index) {
        Error = SetupChildPath(Path, PackagePath, Dlls[Index]);
        if (!Error) Error = KxCfgpValidateSetupBinary(Path, IMAGE_FILE_MACHINE_AMD64, TRUE, Transaction);
        if (Error) return Error;
    }
    for (Index = 0; Index < ARRAYSIZE(Exes); ++Index) {
        Error = SetupChildPath(Path, PackagePath, Exes[Index]);
        if (!Error) Error = KxCfgpValidateSetupBinary(Path, IMAGE_FILE_MACHINE_AMD64, FALSE, Transaction);
        if (Error) return Error;
    }
    for (View = 0; View < 2; ++View) {
        for (Index = 0; Index <= ARRAYSIZE(SetupLibraries); ++Index) {
            BOOL Optional = Index == ARRAYSIZE(SetupLibraries);
            PCWSTR Name = Optional ? L"KxSChanl.dll" : SetupLibraries[Index];
            if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\%s%s", PackagePath, View ? L"Kex32\\" : L"", Name))) return ERROR_FILENAME_EXCED_RANGE;
            Error = Optional ? CheckPackageFile(Path, Transaction, TRUE) : 0;
            if (Optional) Tls[View] = !Error;
            if (Optional && Error == ERROR_FILE_NOT_FOUND) continue;
            if (!Error) Error = KxCfgpValidateSetupBinary(Path, View ? IMAGE_FILE_MACHINE_I386 : IMAGE_FILE_MACHINE_AMD64, TRUE, Transaction);
            if (Error) return Error;
        }
    }
    return Tls[0] == Tls[1] ? 0 : ERROR_FILE_NOT_FOUND;
}

LONG KxCfgpDeploySetupFiles(PCWSTR Package, PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction)
{
    WCHAR TargetPath[MAX_PATH], NativePath[MAX_PATH], WowPath[MAX_PATH], PackagePath[MAX_PATH];
    WCHAR Source[MAX_PATH], Destination[MAX_PATH]; unsigned Index, View;
    PCWSTR Paths[] = {Target, NativeSystem, WowSystem, Package};
    PWSTR FullPaths[] = {TargetPath, NativePath, WowPath, PackagePath};
    PCWSTR Required[] = {L"KexShlEx.dll", L"VistaRun.exe", L"KexCfg.exe", L"VxlView.exe",
        L"install.bat", L"Remove-VxKex-Files.cmd", L"Kex64\\dwrw10.dll"};
    BOOL Tls[2]; LONG Error = ValidateSetupRoots(Paths, FullPaths, ARRAYSIZE(Paths), Transaction);
    if (Error) return Error;
    // Preflight every required file before staging the first destination change.
    for (Index = 0; Index < ARRAYSIZE(Required); ++Index) {
        Error = SetupChildPath(Source, PackagePath, Required[Index]);
        if (!Error) Error = CheckPackageFile(Source, Transaction, FALSE);
        if (Error) return Error;
    }
    for (View = 0; View < 2; ++View) {
        for (Index = 0; Index < ARRAYSIZE(SetupLibraries); ++Index) {
            if (FAILED(StringCchPrintf(Source, ARRAYSIZE(Source), L"%s\\%s%s", PackagePath,
                View ? L"Kex32\\" : L"", SetupLibraries[Index]))) return ERROR_FILENAME_EXCED_RANGE;
            Error = CheckPackageFile(Source, Transaction, FALSE);
            if (Error) return Error;
        }
        if (FAILED(StringCchPrintf(Source, ARRAYSIZE(Source), L"%s\\%sKxSChanl.dll", PackagePath,
            View ? L"Kex32\\" : L""))) return ERROR_FILENAME_EXCED_RANGE;
        Error = CheckPackageFile(Source, Transaction, TRUE);
        if (Error && Error != ERROR_FILE_NOT_FOUND) return Error;
        Tls[View] = !Error;
    }
    // A bundled TLS provider must cover both process architectures.
    if (Tls[0] != Tls[1]) return ERROR_FILE_NOT_FOUND;
    Error = KxCfgpCopySetupDirectory(PackagePath, TargetPath, Transaction);
    if (Error) return Error;
    for (View = 0; View < 2; ++View) {
        for (Index = 0; Index <= ARRAYSIZE(SetupLibraries); ++Index) {
            PCWSTR Name = Index == ARRAYSIZE(SetupLibraries) ? L"KxSChanl.dll" : SetupLibraries[Index];
            if (Index == ARRAYSIZE(SetupLibraries) && !Tls[View]) continue;
            if (FAILED(StringCchPrintf(Source, ARRAYSIZE(Source), L"%s\\%s%s", PackagePath,
                View ? L"Kex32\\" : L"", Name))) return ERROR_FILENAME_EXCED_RANGE;
            Error = SetupChildPath(Destination, View ? WowPath : NativePath, Name);
            if (!Error) Error = KxCfgpCopySetupFile(Source, Destination, Transaction);
            if (Error) return Error;
        }
    }
    return 0;
}

LONG KxCfgpRemoveSetupFiles(PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction)
{
    WCHAR TargetPath[MAX_PATH], NativePath[MAX_PATH], WowPath[MAX_PATH], Path[MAX_PATH];
    PCWSTR Paths[] = {Target, NativeSystem, WowSystem};
    PWSTR FullPaths[] = {TargetPath, NativePath, WowPath}; unsigned Index, View;
    LONG Error = ValidateSetupRoots(Paths, FullPaths, ARRAYSIZE(Paths), Transaction);
    if (Error) return Error;
    // System directories are shared: remove known product files only.
    for (View = 0; View < 2; ++View) {
        for (Index = 0; Index <= ARRAYSIZE(SetupLibraries); ++Index) {
            Error = SetupChildPath(Path, View ? WowPath : NativePath,
                Index == ARRAYSIZE(SetupLibraries) ? L"KxSChanl.dll" : SetupLibraries[Index]);
            if (!Error) Error = KxCfgpDeleteSetupFile(Path, Transaction);
            if (Error) return Error;
        }
    }
    return KxCfgpRemoveSetupDirectory(TargetPath, Transaction);
}

// Production entry point: validation and staging share the same transaction.
LONG KxCfgpDeploySetupPackage(PCWSTR Package, PCWSTR Target, PCWSTR NativeSystem,
    PCWSTR WowSystem, HANDLE Transaction)
{
    LONG Error = KxCfgpValidateSetupPackage(Package, Transaction);
    if (Error) return Error;
    return KxCfgpDeploySetupFiles(Package, Target, NativeSystem, WowSystem, Transaction);
}
