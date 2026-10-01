#include "kexcfg.h"
#include <ktmw32.h>

// Recheck after elevation: inaccessible or disconnected files are never stale.
static BOOL CanClean(PCWSTR Path)
{
    WCHAR Root[MAX_PATH]; DWORD Error;
    if (GetFileAttributes(Path) != INVALID_FILE_ATTRIBUTES) return FALSE;
    Error = GetLastError();
    if (Error != ERROR_FILE_NOT_FOUND && Error != ERROR_PATH_NOT_FOUND) return FALSE;
    if (PathIsNetworkPath(Path)) return FALSE;
    if (FAILED(StringCchCopy(Root, ARRAYSIZE(Root), Path)) ||
        FAILED(PathCchStripToRoot(Root, ARRAYSIZE(Root)))) return FALSE;
    return GetDriveType(Root) == DRIVE_FIXED;
}

DWORD KexCfgApplyBulkArguments(int Count, PWSTR *Args)
{
    int Index; BOOL Add = !_wcsicmp(Args[1], L"/ADD");
    BOOL Clean = !_wcsicmp(Args[1], L"/CLEAN");
    HANDLE Transaction; DWORD Error = ERROR_SUCCESS;
    KXCFG_PROGRAM_CONFIGURATION Configuration = {0};
    if (Count < 3) return ERROR_INVALID_PARAMETER;
    // Validate the entire request before opening a transaction or writing.
    for (Index = 2; Index < Count; ++Index) {
        PCWSTR Path = Args[Index];
        if (!*Path || wcslen(Path) >= MAX_PATH || PathIsRelative(Path) ||
            wcschr(Path, L'"')) return ERROR_INVALID_PARAMETER;
        if (Add) {
            DWORD Attributes = GetFileAttributes(Path);
            if (Attributes == INVALID_FILE_ATTRIBUTES) return GetLastError();
            if (Attributes & FILE_ATTRIBUTE_DIRECTORY) return ERROR_DIRECTORY;
        }
    }
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) return GetLastError();
    Configuration.Enabled = TRUE;
    for (Index = 2; Index < Count; ++Index) {
        BOOL Success;
        if (Clean && !CanClean(Args[Index])) continue;
        Success = Add ? KxCfgSetConfiguration(Args[Index], &Configuration, Transaction) :
            KxCfgDeleteConfiguration(Args[Index], Transaction);
        if (!Success) { Error = GetLastError(); if (!Error) Error = ERROR_GEN_FAILURE; break; }
    }
    if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
    if (Error && !RollbackTransaction(Transaction)) {
        // Preserve the original failure; callers must not report a successful save.
        OutputDebugString(L"VxKex bulk configuration rollback failed.\n");
    }
    CloseHandle(Transaction); return Error;
}
