#include "buildcfg.h"
#include <KxCfgHlp.h>

#define VXL_PROGID L"VxKexVista.Log"

static LONG ReadString(HKEY Key, PCWSTR Name, PWSTR Buffer, DWORD Characters)
{
    DWORD Type = 0, Bytes = Characters * sizeof(WCHAR);
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Buffer, &Bytes);
    if (Error) return Error;
    if (Type != REG_SZ || Bytes < sizeof(WCHAR) || (Bytes & 1) ||
        Bytes > Characters * sizeof(WCHAR) || Buffer[Bytes / sizeof(WCHAR) - 1])
        return ERROR_INVALID_DATA;
    return ERROR_SUCCESS;
}

static LONG UpdateString(HKEY Root, HANDLE Transaction, PCWSTR Subkey,
    PCWSTR Name, PCWSTR Value, BOOLEAN Install)
{
    HKEY Key;
    LONG Error;
    WCHAR Current[1024];
    if (Install) Error = RegCreateKeyTransacted(Root, Subkey, 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &Key, NULL, Transaction, NULL);
    else Error = RegOpenKeyTransacted(Root, Subkey, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY,
        &Key, Transaction, NULL);
    if (!Install && Error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (Error) return Error;
    if (Install) Error = RegSetValueEx(Key, Name, 0, REG_SZ, (PCBYTE)Value,
        (DWORD)((wcslen(Value) + 1) * sizeof(WCHAR)));
    else {
        Error = ReadString(Key, Name, Current, ARRAYSIZE(Current));
        if (Error == ERROR_FILE_NOT_FOUND) Error = ERROR_SUCCESS;
        else if (!Error && !wcscmp(Current, Value)) Error = RegDeleteValue(Key, Name);
        // A value changed by another application belongs to that application.
    }
    RegCloseKey(Key);
    return Error;
}

static LONG DeleteEmptyKey(HKEY Root, HANDLE Transaction, PCWSTR Subkey)
{
    HKEY Key;
    DWORD Subkeys, Values;
    LONG Error = RegOpenKeyTransacted(Root, Subkey, 0, KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (Error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (Error) return Error;
    Error = RegQueryInfoKey(Key, NULL, NULL, NULL, &Subkeys, NULL, NULL,
        &Values, NULL, NULL, NULL, NULL);
    RegCloseKey(Key);
    if (!Error && !Subkeys && !Values)
        Error = RegDeleteKeyTransacted(Root, Subkey, KEY_WOW64_64KEY, 0, Transaction, NULL);
    return Error;
}

// ClassesRoot can be an isolated registry root in the command-based VM probe.
// Caller owns the transaction and must roll back on any failure.
LONG KxCfgpUpdateLogAssociation(HKEY ClassesRoot, PCWSTR Executable, BOOLEAN Install, HANDLE Transaction)
{
    HKEY Key;
    LONG Error;
    WCHAR Owner[MAX_PATH], Command[1024], Current[1024], Icon[MAX_PATH + 24];
    DWORD Subkeys, Values;
    ULONG Index;
    PCWSTR EmptyKeys[] = { VXL_PROGID L"\\shell\\open\\command",
        VXL_PROGID L"\\shell\\open", VXL_PROGID L"\\shell", VXL_PROGID L"\\DefaultIcon",
        VXL_PROGID, L".vxl\\OpenWithProgids", L".vxl" };
    if (!Executable || !Executable[0] || wcschr(Executable, L'"') ||
        wcslen(Executable) >= MAX_PATH) return ERROR_INVALID_PARAMETER;
    StringCchPrintf(Command, ARRAYSIZE(Command), L"\"%s\" \"%%1\"", Executable);
    StringCchPrintf(Icon, ARRAYSIZE(Icon), L"\"%s\",1", Executable);
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    Error = RegOpenKeyTransacted(ClassesRoot, VXL_PROGID, 0, KEY_READ | KEY_WOW64_64KEY,
        &Key, Transaction, NULL);
    if (!Error) {
        Error = ReadString(Key, L"VxKexOwnerPath", Owner, ARRAYSIZE(Owner));
        if (Error == ERROR_FILE_NOT_FOUND) {
            Error = RegQueryInfoKey(Key, NULL, NULL, NULL, &Subkeys, NULL, NULL,
                &Values, NULL, NULL, NULL, NULL);
            if (!Error && (Subkeys || Values)) Error = ERROR_ALREADY_EXISTS;
        } else if (!Error && _wcsicmp(Owner, Executable)) Error = ERROR_ALREADY_EXISTS;
        RegCloseKey(Key);
        if (Error) goto Finished;
        Error = RegOpenKeyTransacted(ClassesRoot, VXL_PROGID L"\\shell\\open\\command",
            0, KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
        if (!Error) {
            Error = ReadString(Key, NULL, Current, ARRAYSIZE(Current));
            if (!Error && wcscmp(Current, Command)) Error = ERROR_ALREADY_EXISTS;
            RegCloseKey(Key);
        }
        if (Error != ERROR_SUCCESS && Error != ERROR_FILE_NOT_FOUND) goto Finished;
    } else if (Error == ERROR_FILE_NOT_FOUND) {
        if (!Install) { Error = ERROR_SUCCESS; goto Finished; }
    } else goto Finished;

#define UPDATE(Subkey, Name, Value) \
    Error = UpdateString(ClassesRoot, Transaction, Subkey, Name, Value, Install); \
    if (Error) goto Finished
    UPDATE(VXL_PROGID, L"VxKexOwnerPath", Executable);
    UPDATE(VXL_PROGID, NULL, L"VxKex Vista Log File");
    UPDATE(VXL_PROGID L"\\shell\\open\\command", NULL, Command);
    UPDATE(VXL_PROGID L"\\DefaultIcon", NULL, Icon);
    UPDATE(L".vxl\\OpenWithProgids", VXL_PROGID, L"");

    Error = RegOpenKeyTransacted(ClassesRoot, L".vxl", 0, KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (!Error) {
        Error = ReadString(Key, NULL, Current, ARRAYSIZE(Current));
        RegCloseKey(Key);
    }
    if (Error == ERROR_FILE_NOT_FOUND || (!Error &&
        (!Current[0] || !wcscmp(Current, VXL_PROGID)))) {
        UPDATE(L".vxl", NULL, VXL_PROGID);
    } else if (Error) goto Finished;
    // Keep an existing foreign default association and offer Open With instead.
    Error = ERROR_SUCCESS;
    if (!Install) for (Index = 0; Index < ARRAYSIZE(EmptyKeys); ++Index) {
        Error = DeleteEmptyKey(ClassesRoot, Transaction, EmptyKeys[Index]);
        if (Error) break;
    }
#undef UPDATE
Finished:
    return Error;
}
