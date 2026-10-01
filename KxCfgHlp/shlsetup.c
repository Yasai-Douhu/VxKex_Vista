#include "buildcfg.h"
#include <KxCfgHlp.h>

#define SHELL_CLSID L"{9AACA888-A5F5-4C01-852E-8A2005C1D45F}"
#define CLASS_KEY L"CLSID\\" SHELL_CLSID
#define APPROVED_KEY L"Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"

static LONG ReadOwnedString(HKEY Key, PCWSTR Name, PCWSTR Expected, PBOOL Matches)
{
    WCHAR Value[MAX_PATH + 32]; DWORD Bytes = sizeof(Value), Type;
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Value, &Bytes);
    *Matches = FALSE;
    if (Error) return Error;
    if (Type != REG_SZ || Bytes < sizeof(WCHAR) || Bytes > sizeof(Value) ||
        (Bytes & 1) || Value[Bytes / sizeof(WCHAR) - 1]) return ERROR_INVALID_DATA;
    *Matches = !_wcsicmp(Value, Expected); return 0;
}

static LONG UpdateOwnedString(HKEY Root, PCWSTR Path, PCWSTR Name, PCWSTR Value,
    BOOLEAN Install, HANDLE Transaction)
{
    HKEY Key; LONG Error; BOOL Matches;
    if (Install) Error = RegCreateKeyTransacted(Root, Path, 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &Key, NULL, Transaction, NULL);
    else Error = RegOpenKeyTransacted(Root, Path, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY,
        &Key, Transaction, NULL);
    if (!Install && Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = ReadOwnedString(Key, Name, Value, &Matches);
    if (Install) {
        if (Error == ERROR_FILE_NOT_FOUND || (!Error && Matches))
            Error = RegSetValueEx(Key, Name, 0, REG_SZ, (PCBYTE)Value,
                (DWORD)((wcslen(Value) + 1) * sizeof(WCHAR)));
        else if (!Error) Error = ERROR_ALREADY_EXISTS;
    } else {
        if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
        else if (!Error && Matches) Error = RegDeleteValue(Key, Name);
        // Retain foreign values in a shared registration key.
    }
    RegCloseKey(Key); return Error;
}

static LONG RemoveEmpty(HKEY Root, PCWSTR Path, HANDLE Transaction)
{
    HKEY Key; DWORD Subkeys, Values; LONG Error;
    Error = RegOpenKeyTransacted(Root, Path, 0, KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = RegQueryInfoKey(Key, NULL, NULL, NULL, &Subkeys, NULL, NULL,
        &Values, NULL, NULL, NULL, NULL);
    RegCloseKey(Key);
    if (!Error && !Subkeys && !Values)
        Error = RegDeleteKeyTransacted(Root, Path, KEY_WOW64_64KEY, 0, Transaction, NULL);
    return Error;
}

// Roots select the native machine view or an isolated test fixture. Every
// operation is transacted; the caller must roll back on any nonzero result.
LONG KxCfgpUpdateShellExtension(HKEY ClassesRoot, HKEY SoftwareRoot,
    PCWSTR DllPath, BOOLEAN Install, HANDLE Transaction)
{
    HKEY Key; LONG Error; BOOL Matches; unsigned Index;
    PCWSTR HandlerKeys[] = {L"exefile\\shellex\\PropertySheetHandlers\\VxKex",
        L"lnkfile\\shellex\\PropertySheetHandlers\\VxKex"};
    if (!ClassesRoot || !SoftwareRoot || !Transaction || Transaction == INVALID_HANDLE_VALUE ||
        !DllPath || wcslen(DllPath) < 4 || wcslen(DllPath) >= MAX_PATH ||
        PathIsRelative(DllPath) || wcschr(DllPath, L'"')) return ERROR_INVALID_PARAMETER;
    // A changed server path must never be adopted or removed as our own.
    Error = RegOpenKeyTransacted(ClassesRoot, CLASS_KEY L"\\InProcServer32", 0,
        KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (!Error) {
        Error = ReadOwnedString(Key, NULL, DllPath, &Matches);
        RegCloseKey(Key);
        if (!Error && !Matches) Error = ERROR_ALREADY_EXISTS;
        if (Error && Error != ERROR_FILE_NOT_FOUND) return Error;
    } else if (Error != ERROR_FILE_NOT_FOUND) return Error;
#define UPDATE(Root, Path, Name, Value) \
    Error = UpdateOwnedString(Root, Path, Name, Value, Install, Transaction); \
    if (Error) return Error
    UPDATE(ClassesRoot, CLASS_KEY, NULL, L"VxKex Property Sheet Handler");
    UPDATE(ClassesRoot, CLASS_KEY L"\\InProcServer32", NULL, DllPath);
    UPDATE(ClassesRoot, CLASS_KEY L"\\InProcServer32", L"ThreadingModel", L"Apartment");
    UPDATE(SoftwareRoot, APPROVED_KEY, SHELL_CLSID, L"VxKex Property Sheet Handler");
    for (Index = 0; Index < ARRAYSIZE(HandlerKeys); ++Index) {
        UPDATE(ClassesRoot, HandlerKeys[Index], NULL, SHELL_CLSID);
    }
#undef UPDATE
    if (!Install) {
        for (Index = 0; Index < ARRAYSIZE(HandlerKeys); ++Index) {
            Error = RemoveEmpty(ClassesRoot, HandlerKeys[Index], Transaction);
            if (Error) return Error;
        }
        Error = RemoveEmpty(ClassesRoot, CLASS_KEY L"\\InProcServer32", Transaction);
        if (Error) return Error;
        Error = RemoveEmpty(ClassesRoot, CLASS_KEY, Transaction);
        if (Error) return Error;
        // Approved is a shared Windows key and is deliberately retained.
    }
    return 0;
}
