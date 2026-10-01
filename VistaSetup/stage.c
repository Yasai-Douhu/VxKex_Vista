#include "buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <sddl.h>
#include <Aclapi.h>
#include <stdio.h>

static const char Marker[] = "VxKex Vista setup workspace v1\r\n";
typedef struct _CACHE_COMPLETION {
    DWORD Magic, Version, ProcessId;
    FILETIME CreationTime;
} CACHE_COMPLETION;
static DWORD CheckParents(PCWSTR Path)
{
    WCHAR Full[MAX_PATH], Saved; DWORD Length, Index, Attributes, Error;
    if (!Path || wcslen(Path) < 4 || Path[1] != L':' || Path[2] != L'\\' || wcschr(Path + 2, L':')) return ERROR_INVALID_NAME;
    Length = GetFullPathName(Path, ARRAYSIZE(Full), Full, NULL);
    if (!Length || Length >= ARRAYSIZE(Full)) return ERROR_BAD_PATHNAME;
    for (Index = 3; Index <= Length; ++Index) {
        if (Full[Index] && Full[Index] != L'\\') continue;
        Saved = Full[Index]; Full[Index] = 0;
        Attributes = GetFileAttributes(Full); Error = GetLastError(); Full[Index] = Saved;
        if (Attributes == INVALID_FILE_ATTRIBUTES) {
            if (!Saved && Error == ERROR_FILE_NOT_FOUND) return 0;
            return Error;
        }
        if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY) || (Attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return ERROR_INVALID_DATA;
    }
    return 0;
}
static DWORD CheckRootSecurity(HANDLE Root)
{
    PSECURITY_DESCRIPTOR Descriptor = NULL; PACL Acl = NULL; PSID Owner;
    BYTE System[SECURITY_MAX_SID_SIZE], Admin[SECURITY_MAX_SID_SIZE]; DWORD Size, Error, Revision, Index;
    SECURITY_DESCRIPTOR_CONTROL Control; BOOL SystemFound = FALSE, AdminFound = FALSE;
    Size = sizeof(System); if (!CreateWellKnownSid(WinLocalSystemSid, NULL, System, &Size)) return GetLastError();
    Size = sizeof(Admin); if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, Admin, &Size)) return GetLastError();
    Error = GetSecurityInfo(Root, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &Owner, NULL, &Acl, NULL, &Descriptor);
    if (Error) return Error;
    if (!Owner || (!EqualSid(Owner, Admin) && !EqualSid(Owner, System)) || !Acl || Acl->AceCount != 2 ||
        !GetSecurityDescriptorControl(Descriptor, &Control, &Revision) || !(Control & SE_DACL_PROTECTED)) Error = ERROR_ACCESS_DENIED;
    for (Index = 0; !Error && Index < Acl->AceCount; ++Index) {
        PACCESS_ALLOWED_ACE Ace;
        if (!GetAce(Acl, Index, (PVOID *)&Ace) || Ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE ||
            (Ace->Mask & FILE_ALL_ACCESS) != FILE_ALL_ACCESS ||
            (Ace->Header.AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)) != (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) ||
            (Ace->Header.AceFlags & INHERIT_ONLY_ACE)) { Error = ERROR_ACCESS_DENIED; break; }
        if (EqualSid(&Ace->SidStart, System)) SystemFound = TRUE;
        else if (EqualSid(&Ace->SidStart, Admin)) AdminFound = TRUE;
        else Error = ERROR_ACCESS_DENIED;
    }
    if (!SystemFound || !AdminFound) Error = ERROR_ACCESS_DENIED;
    LocalFree(Descriptor); return Error;
}

static BOOL PackageName(PCWSTR Name)
{
    GUID Guid;
    return wcslen(Name) == 46 && !wcsncmp(Name, L"Package-", 8) &&
        Name[8] == L'{' && Name[45] == L'}' && SUCCEEDED(CLSIDFromString((PWSTR)(Name + 8), &Guid));
}
static DWORD OpenOwnedCache(PCWSTR CacheRoot, HANDLE Transaction, PHANDLE Root)
{
    WCHAR Path[MAX_PATH]; HANDLE File; char Data[sizeof(Marker)]; DWORD Error, Count;
    Error = CheckParents(CacheRoot); if (Error) return Error;
    *Root = CreateFileTransacted(CacheRoot, READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL, Transaction, NULL, NULL);
    if (*Root == INVALID_HANDLE_VALUE) return GetLastError();
    Error = CheckRootSecurity(*Root); if (Error) return Error;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Owner.txt", CacheRoot))) return ERROR_BAD_PATHNAME;
    File = CreateFileTransacted(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
    if (File == INVALID_HANDLE_VALUE) return GetLastError();
    {
        BY_HANDLE_FILE_INFORMATION Info;
        if (!GetFileInformationByHandle(File, &Info)) Error = GetLastError();
        else if ((Info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) || Info.nNumberOfLinks != 1) Error = ERROR_INVALID_DATA;
        else if (!ReadFile(File, Data, sizeof(Data), &Count, NULL)) Error = GetLastError();
        else if (Count != sizeof(Marker) - 1 || memcmp(Data, Marker, Count)) Error = ERROR_INVALID_DATA;
    }
    CloseHandle(File); return Error;
}
static DWORD PackagePath(PCWSTR CacheRoot, PCWSTR Package, PWSTR Full)
{
    WCHAR Root[MAX_PATH]; DWORD Length, Size;
    Size = GetFullPathName(CacheRoot, ARRAYSIZE(Root), Root, NULL);
    Length = GetFullPathName(Package, MAX_PATH, Full, NULL);
    if (!Size || Size >= ARRAYSIZE(Root) || !Length || Length >= MAX_PATH) return ERROR_BAD_PATHNAME;
    if (Length <= Size + 1 || _wcsnicmp(Root, Full, Size) || Full[Size] != L'\\' ||
        !PackageName(Full + Size + 1)) return ERROR_INVALID_NAME;
    return CheckParents(Full);
}

// Completion is deliberately separate from the installation transaction. A
// record authorizes later collection only after the owning dispatcher exits.
DWORD VistaSetupCompleteCache(PCWSTR CacheRoot, PCWSTR Package, HANDLE OwnerProcess)
{
    HANDLE Transaction, Root = INVALID_HANDLE_VALUE, File = INVALID_HANDLE_VALUE;
    WCHAR Full[MAX_PATH], Path[MAX_PATH]; CACHE_COMPLETION Stamp = {0};
    FILETIME Exit, Kernel, User; DWORD Error, Count;
    PSECURITY_DESCRIPTOR Descriptor = NULL; SECURITY_ATTRIBUTES Security = {sizeof(Security)};
    if (!CacheRoot || !Package || !OwnerProcess) return ERROR_INVALID_PARAMETER;
    Error = PackagePath(CacheRoot, Package, Full); if (Error) return Error;
    Stamp.Magic = 0x43584b56; Stamp.Version = 1; Stamp.ProcessId = GetProcessId(OwnerProcess);
    if (!Stamp.ProcessId || !GetProcessTimes(OwnerProcess, &Stamp.CreationTime, &Exit, &Kernel, &User)) return GetLastError();
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
        L"O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &Descriptor, NULL)) return GetLastError();
    Security.lpSecurityDescriptor = Descriptor;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) { Error = GetLastError(); LocalFree(Descriptor); return Error; }
    Error = OpenOwnedCache(CacheRoot, Transaction, &Root); if (Error) goto Done;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Completed.bin", Full))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    File = CreateFileTransacted(Path, GENERIC_WRITE, 0, &Security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
        NULL, Transaction, NULL, NULL);
    if (File == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
    if (!WriteFile(File, &Stamp, sizeof(Stamp), &Count, NULL) || Count != sizeof(Stamp)) {
        Error = GetLastError(); if (!Error) Error = ERROR_WRITE_FAULT;
    }
Done:
    if (File != INVALID_HANDLE_VALUE) CloseHandle(File);
    if (Root != INVALID_HANDLE_VALUE) CloseHandle(Root);
    if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
    if (Error && !RollbackTransaction(Transaction)) fwprintf(stderr, L"Completion rollback failed: %lu.\n", GetLastError());
    CloseHandle(Transaction); LocalFree(Descriptor); return Error;
}
static DWORD CollectPackage(PCWSTR CacheRoot, PCWSTR Package)
{
    HANDLE Transaction, Root = INVALID_HANDLE_VALUE, File = INVALID_HANDLE_VALUE, Process;
    WCHAR Full[MAX_PATH], Path[MAX_PATH]; CACHE_COMPLETION Stamp; DWORD Error, Count;
    FILETIME Creation, Exit, Kernel, User; BY_HANDLE_FILE_INFORMATION Info;
    Error = PackagePath(CacheRoot, Package, Full); if (Error) return Error;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) return GetLastError();
    Error = OpenOwnedCache(CacheRoot, Transaction, &Root); if (Error) goto Done;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Completed.bin", Full))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    File = CreateFileTransacted(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
    if (File == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
    if (!GetFileInformationByHandle(File, &Info)) { Error = GetLastError(); goto Done; }
    if ((Info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        Info.nNumberOfLinks != 1 || Info.nFileSizeHigh || Info.nFileSizeLow != sizeof(Stamp)) { Error = ERROR_INVALID_DATA; goto Done; }
    if (!ReadFile(File, &Stamp, sizeof(Stamp), &Count, NULL)) { Error = GetLastError(); goto Done; }
    if (Count != sizeof(Stamp) || Stamp.Magic != 0x43584b56 || Stamp.Version != 1 || !Stamp.ProcessId) { Error = ERROR_INVALID_DATA; goto Done; }
    CloseHandle(File); File = INVALID_HANDLE_VALUE;
    Process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Stamp.ProcessId);
    if (Process) {
        if (!GetProcessTimes(Process, &Creation, &Exit, &Kernel, &User)) Error = GetLastError();
        else if (!memcmp(&Creation, &Stamp.CreationTime, sizeof(Creation)) &&
            WaitForSingleObject(Process, 0) != WAIT_OBJECT_0) Error = ERROR_BUSY;
        CloseHandle(Process); if (Error) goto Done;
    } else if (GetLastError() != ERROR_INVALID_PARAMETER) { Error = GetLastError(); goto Done; }
    Error = KxCfgpRemoveSetupDirectory(Full, Transaction);
Done:
    if (File != INVALID_HANDLE_VALUE) CloseHandle(File);
    if (Root != INVALID_HANDLE_VALUE) CloseHandle(Root);
    if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
    if (Error && !RollbackTransaction(Transaction)) fwprintf(stderr, L"Collection rollback failed: %lu.\n", GetLastError());
    CloseHandle(Transaction); return Error;
}
DWORD VistaSetupCollectCache(PCWSTR CacheRoot, PCWSTR KeepPackage)
{
    WCHAR Search[MAX_PATH], Child[MAX_PATH], Keep[MAX_PATH]; HANDLE Find; WIN32_FIND_DATA Data;
    DWORD Error;
    if (!CacheRoot) return ERROR_INVALID_PARAMETER;
    Error = CheckParents(CacheRoot); if (Error) return Error;
    Keep[0] = 0;
    if (KeepPackage) { Error = PackagePath(CacheRoot, KeepPackage, Keep); if (Error) return Error; }
    if (FAILED(StringCchPrintf(Search, ARRAYSIZE(Search), L"%s\\Package-*", CacheRoot))) return ERROR_BAD_PATHNAME;
    Find = FindFirstFile(Search, &Data);
    if (Find == INVALID_HANDLE_VALUE) { Error = GetLastError(); return Error == ERROR_FILE_NOT_FOUND ? 0 : Error; }
    do {
        if (!PackageName(Data.cFileName) || !(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if (FAILED(StringCchPrintf(Child, ARRAYSIZE(Child), L"%s\\%s", CacheRoot, Data.cFileName))) continue;
        if (Keep[0] && !_wcsicmp(Keep, Child)) continue;
        Error = CollectPackage(CacheRoot, Child);
        if (Error && Error != ERROR_FILE_NOT_FOUND && Error != ERROR_BUSY)
            fwprintf(stderr, L"Cache package retained: %s status=%lu.\n", Child, Error);
    } while (FindNextFile(Find, &Data));
    Error = GetLastError(); FindClose(Find); return Error == ERROR_NO_MORE_FILES ? 0 : Error;
}
static DWORD SecureTree(PCWSTR Path, HANDLE Transaction, PSID Owner, PACL Acl, unsigned Depth)
{
    HANDLE File, Search; WIN32_FIND_DATA Find; WIN32_FILE_ATTRIBUTE_DATA Data;
    WCHAR Child[MAX_PATH]; DWORD Error;
    if (Depth > 32) return ERROR_INVALID_DATA;
    if (!GetFileAttributesTransacted(Path, GetFileExInfoStandard, &Data, Transaction)) return GetLastError();
    if (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_INVALID_DATA;
    File = CreateFileTransacted(Path, READ_CONTROL | WRITE_OWNER | WRITE_DAC, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
    if (File == INVALID_HANDLE_VALUE) { Error = GetLastError(); fwprintf(stderr, L"Cache secure-open %s status=%lu.\n", Path, Error); return Error; }
    Error = SetSecurityInfo(File, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
        PROTECTED_DACL_SECURITY_INFORMATION, Owner, NULL, Acl, NULL);
    CloseHandle(File);
    if (Error) fwprintf(stderr, L"Cache secure-set %s status=%lu.\n", Path, Error);
    if (Error || !(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return Error;
    if (FAILED(StringCchPrintf(Child, ARRAYSIZE(Child), L"%s\\*", Path))) return ERROR_BAD_PATHNAME;
    Search = FindFirstFileTransacted(Child, FindExInfoStandard, &Find, FindExSearchNameMatch, NULL, 0, Transaction);
    if (Search == INVALID_HANDLE_VALUE) { Error = GetLastError(); return Error == ERROR_FILE_NOT_FOUND ? 0 : Error; }
    Error = 0;
    do {
        if (!wcscmp(Find.cFileName, L".") || !wcscmp(Find.cFileName, L"..")) continue;
        if (FAILED(StringCchPrintf(Child, ARRAYSIZE(Child), L"%s\\%s", Path, Find.cFileName))) { Error = ERROR_BAD_PATHNAME; break; }
        Error = SecureTree(Child, Transaction, Owner, Acl, Depth + 1);
        if (Error) break;
    } while (FindNextFile(Search, &Find));
    if (!Error && GetLastError() != ERROR_NO_MORE_FILES) Error = GetLastError();
    FindClose(Search); return Error;
}

// The caller authorizes CacheRoot. Every run uses a fresh directory, so an old
// helper may remain mapped without being overwritten. No installation is changed.
DWORD VistaSetupStagePackage(PCWSTR Package, PCWSTR CacheRoot, PWSTR Staged, DWORD StagedChars)
{
    HANDLE Transaction, Root = INVALID_HANDLE_VALUE, File = INVALID_HANDLE_VALUE;
    PSECURITY_DESCRIPTOR Descriptor = NULL; SECURITY_ATTRIBUTES Security = {sizeof(Security)};
    WCHAR Path[MAX_PATH], GuidText[40]; GUID Guid; char Data[sizeof(Marker)]; PCWSTR Step = L"create-cache";
    DWORD Error, Count, RollbackError; WIN32_FILE_ATTRIBUTE_DATA DataInfo;
    PSID Owner; PACL Acl; BOOL Present, Defaulted, Existing;
    if (!Package || !CacheRoot || !Staged || !StagedChars) return ERROR_INVALID_PARAMETER;
    Staged[0] = 0;
    Error = CheckParents(CacheRoot); if (Error) return Error;
    Existing = GetFileAttributes(CacheRoot) != INVALID_FILE_ATTRIBUTES;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
        L"O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &Descriptor, NULL)) return GetLastError();
    Security.lpSecurityDescriptor = Descriptor;
    GetSecurityDescriptorOwner(Descriptor, &Owner, &Defaulted);
    GetSecurityDescriptorDacl(Descriptor, &Present, &Acl, &Defaulted);
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Finished; }
    if (!CreateDirectoryTransacted(NULL, CacheRoot, &Security, Transaction)) {
        Error = GetLastError(); if (Error != ERROR_ALREADY_EXISTS) goto Done;
    }
    Root = CreateFileTransacted(CacheRoot, READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
    if (Root == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
    Step = L"check-root-security";
    Error = CheckRootSecurity(Root); if (Error) goto Done;
    Step = L"owner-marker";
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Owner.txt", CacheRoot))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    if (GetFileAttributesTransacted(Path, GetFileExInfoStandard, &DataInfo, Transaction)) {
        if (DataInfo.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) { Error = ERROR_INVALID_DATA; goto Done; }
        File = CreateFileTransacted(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT, NULL, Transaction, NULL, NULL);
        if (File == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
        if (!ReadFile(File, Data, sizeof(Data), &Count, NULL)) { Error = GetLastError(); goto Done; }
        if (Count != sizeof(Marker) - 1 || memcmp(Data, Marker, Count)) { Error = ERROR_ALREADY_EXISTS; goto Done; }
    } else {
        Error = GetLastError(); if (Error != ERROR_FILE_NOT_FOUND) goto Done;
        // An existing cache without a marker belongs to someone else.
        if (Existing) { Error = ERROR_ALREADY_EXISTS; goto Done; }
        File = CreateFileTransacted(Path, GENERIC_WRITE, 0, &Security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
            NULL, Transaction, NULL, NULL);
        if (File == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
        if (!WriteFile(File, Marker, sizeof(Marker) - 1, &Count, NULL) || Count != sizeof(Marker) - 1) { Error = GetLastError(); if (!Error) Error = ERROR_WRITE_FAULT; goto Done; }
    }
    CloseHandle(File); File = INVALID_HANDLE_VALUE;
    Step = L"validate-package";
    Error = KxCfgpValidateSetupPackage(Package, Transaction); if (Error) goto Done;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\VistaSetup.exe", Package))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    Error = KxCfgpValidateSetupBinary(Path, IMAGE_FILE_MACHINE_AMD64, FALSE, Transaction); if (Error) goto Done;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Run-VxKexSetup.cmd", Package))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    if (!GetFileAttributesTransacted(Path, GetFileExInfoStandard, &DataInfo, Transaction)) { Error = GetLastError(); goto Done; }
    if (DataInfo.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) { Error = ERROR_INVALID_DATA; goto Done; }
    if (FAILED(CoCreateGuid(&Guid)) || !StringFromGUID2(&Guid, GuidText, ARRAYSIZE(GuidText))) { Error = ERROR_GEN_FAILURE; goto Done; }
    if (FAILED(StringCchPrintf(Staged, StagedChars, L"%s\\Package-%s", CacheRoot, GuidText))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    Step = L"copy-package";
    Error = KxCfgpCopySetupDirectory(Package, Staged, Transaction); if (Error) goto Done;
    // A previously staged source may carry its old dispatcher completion stamp.
    // Never authorize collection of this new workspace using that old identity.
    Step = L"clear-source-completion";
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Completed.bin", Staged))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    Error = KxCfgpDeleteSetupFile(Path, Transaction); if (Error) goto Done;
    Step = L"validate-staged-package";
    Error = KxCfgpValidateSetupPackage(Staged, Transaction); if (Error) goto Done;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\VistaSetup.exe", Staged))) { Error = ERROR_BAD_PATHNAME; goto Done; }
    Error = KxCfgpValidateSetupBinary(Path, IMAGE_FILE_MACHINE_AMD64, FALSE, Transaction); if (Error) goto Done;
    Step = L"secure-package";
    Error = SecureTree(Staged, Transaction, Owner, Acl, 0);
Done:
    if (File != INVALID_HANDLE_VALUE) CloseHandle(File);
    if (Root != INVALID_HANDLE_VALUE) CloseHandle(Root);
    if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
    if (Error && !RollbackTransaction(Transaction)) {
        RollbackError = GetLastError(); fwprintf(stderr, L"Cache rollback failed: %lu.\n", RollbackError);
    }
    CloseHandle(Transaction);
Finished:
    if (Error) { fwprintf(stderr, L"Cache stage=%s status=%lu.\n", Step, Error); Staged[0] = 0; }
    LocalFree(Descriptor); return Error;
}
