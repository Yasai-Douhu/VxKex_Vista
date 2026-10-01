#include "buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>

#define LABEL L"Run with VxKex Vista enabled"
#define OLDLABEL L"Run with VxKex enabled"
static PCWSTR MenuKeys[] = {L"exefile\\shell\\open_vxkex", L"Msi.Package\\shell\\open_vxkex"};
static LONG StringValue(HKEY Key, PCWSTR Name, PWSTR Value, DWORD Chars)
{
    DWORD Bytes = Chars * sizeof(WCHAR), Type; LONG Error;
    Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Value, &Bytes);
    if (Error) return Error;
    if (Type != REG_SZ || Bytes < sizeof(WCHAR) || (Bytes & 1) || Bytes > Chars * sizeof(WCHAR) ||
        Value[Bytes / sizeof(WCHAR) - 1] || wcslen(Value) != Bytes / sizeof(WCHAR) - 1) return ERROR_INVALID_DATA;
    return 0;
}
static LONG MenuCommand(PCWSTR KexDir, PCWSTR Windows, unsigned Index, PWSTR Command, DWORD Chars, PWSTR Legacy)
{
    if (!KexDir || !Windows || PathIsRelative(KexDir) || PathIsRelative(Windows) ||
        wcslen(KexDir) >= MAX_PATH || wcslen(Windows) >= MAX_PATH || wcschr(KexDir, L'"') || wcschr(Windows, L'"')) return ERROR_INVALID_PARAMETER;
    if (FAILED(StringCchPrintf(Legacy, Chars, L"\"%s\\VxKexLdr.exe\" \"%%1\"", KexDir))) return ERROR_BAD_PATHNAME;
    if (FAILED(Index ? StringCchPrintf(Command, Chars, L"\"%s\\VistaRun.exe\" --with-kex \"%s\\System32\\msiexec.exe\" /i \"%%1\"", KexDir, Windows) :
        StringCchPrintf(Command, Chars, L"\"%s\\VistaRun.exe\" --with-kex \"%%1\"", KexDir))) return ERROR_BAD_PATHNAME;
    return 0;
}
static LONG OwnedMenu(HKEY Menu, PCWSTR Expected, PCWSTR Legacy, HANDLE Transaction, PBOOL Owned)
{
    WCHAR Label[128], Command[MAX_PATH * 2 + 64]; HKEY Key; LONG Error;
    *Owned = FALSE;
    Error = StringValue(Menu, NULL, Label, ARRAYSIZE(Label));
    if (Error == ERROR_FILE_NOT_FOUND) {
        DWORD Subkeys, Values;
        Error = RegQueryInfoKey(Menu, NULL, NULL, NULL, &Subkeys, NULL, NULL, &Values, NULL, NULL, NULL, NULL);
        if (!Error) *Owned = !Subkeys && !Values;
        return Error;
    }
    if (Error) return Error;
    if (wcscmp(Label, LABEL) && wcscmp(Label, OLDLABEL)) return 0;
    Error = Transaction ? RegOpenKeyTransacted(Menu, L"command", 0, KEY_READ, &Key, Transaction, NULL) : RegOpenKeyEx(Menu, L"command", 0, KEY_READ, &Key);
    if (Error == ERROR_FILE_NOT_FOUND) { *Owned = TRUE; return 0; }
    if (Error) return Error;
    Error = StringValue(Key, NULL, Command, ARRAYSIZE(Command)); RegCloseKey(Key);
    if (Error == ERROR_FILE_NOT_FOUND) { *Owned = TRUE; return 0; }
    if (!Error) *Owned = !_wcsicmp(Command, Expected) || !_wcsicmp(Command, Legacy);
    return Error;
}
static LONG RemoveEmptyMenuKey(HKEY Root, PCWSTR Path, HANDLE Transaction)
{
    HKEY Key; DWORD Subkeys, Values; LONG Error;
    Error = RegOpenKeyTransacted(Root, Path, 0, KEY_READ | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = RegQueryInfoKey(Key, NULL, NULL, NULL, &Subkeys, NULL, NULL, &Values, NULL, NULL, NULL, NULL); RegCloseKey(Key);
    if (!Error && !Subkeys && !Values) Error = RegDeleteKeyTransacted(Root, Path, KEY_WOW64_64KEY, 0, Transaction, NULL);
    return Error;
}
LONG KxCfgpUpdateContextMenu(HKEY Classes, PCWSTR KexDir, PCWSTR Windows, BOOLEAN Enable, BOOLEAN Extended, HANDLE Transaction)
{
    unsigned Index; HKEY Menu, Key; LONG Error; BOOL Owned; WCHAR Command[MAX_PATH * 2 + 64], Legacy[MAX_PATH * 2 + 64], Value[8], Path[100];
    if (!Classes || !Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    for (Index = 0; Index < ARRAYSIZE(MenuKeys); ++Index) {
        Error = MenuCommand(KexDir, Windows, Index, Command, ARRAYSIZE(Command), Legacy); if (Error) return Error;
        Error = RegOpenKeyTransacted(Classes, MenuKeys[Index], 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Menu, Transaction, NULL);
        if (Error == ERROR_FILE_NOT_FOUND) {
            if (!Enable) continue;
            Error = RegCreateKeyTransacted(Classes, MenuKeys[Index], 0, NULL, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &Menu, NULL, Transaction, NULL);
        }
        if (Error) return Error;
        Error = OwnedMenu(Menu, Command, Legacy, Transaction, &Owned);
        if (!Error && !Owned) Error = Enable ? ERROR_ALREADY_EXISTS : 0;
        if (Error || !Owned) { RegCloseKey(Menu); if (Error) return Error; continue; }
        Error = StringValue(Menu, L"Extended", Value, ARRAYSIZE(Value));
        if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
        else if (!Error && Value[0]) Error = ERROR_ALREADY_EXISTS;
        if (Error) { RegCloseKey(Menu); return Error; }
        if (Enable) {
            Error = RegCreateKeyTransacted(Menu, L"command", 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &Key, NULL, Transaction, NULL);
            if (!Error) { Error = RegWriteString(Key, NULL, NULL, Command); RegCloseKey(Key); }
            if (!Error) Error = RegWriteString(Menu, NULL, NULL, LABEL);
            if (!Error && Extended) Error = RegWriteString(Menu, NULL, L"Extended", L"");
            if (!Error && !Extended) { Error = RegDeleteValue(Menu, L"Extended"); if (Error == ERROR_FILE_NOT_FOUND) Error = 0; }
        } else {
            Error = RegOpenKeyTransacted(Menu, L"command", 0, KEY_READ | KEY_WRITE, &Key, Transaction, NULL);
            if (!Error) { Error = RegDeleteValue(Key, NULL); if (Error == ERROR_FILE_NOT_FOUND) Error = 0; RegCloseKey(Key); }
            else if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
            if (!Error) { Error = RegDeleteValue(Menu, NULL); if (Error == ERROR_FILE_NOT_FOUND) Error = 0; }
            if (!Error) { Error = RegDeleteValue(Menu, L"Extended"); if (Error == ERROR_FILE_NOT_FOUND) Error = 0; }
        }
        RegCloseKey(Menu); if (Error) return Error;
        if (!Enable) {
            StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\command", MenuKeys[Index]);
            Error = RemoveEmptyMenuKey(Classes, Path, Transaction);
            if (!Error) Error = RemoveEmptyMenuKey(Classes, MenuKeys[Index], Transaction);
            if (Error) return Error;
        }
    }
    return 0;
}
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryShellContextMenuEntries(PBOOLEAN ExtendedMenu)
{
    HKEY Menu, Key; WCHAR KexDir[MAX_PATH], Windows[MAX_PATH], Command[MAX_PATH * 2 + 64], Legacy[MAX_PATH * 2 + 64], Value[MAX_PATH * 2 + 64]; LONG Error; BOOL Owned;
    if (ExtendedMenu) *ExtendedMenu = FALSE;
    if (!KxCfgGetKexDir(KexDir, ARRAYSIZE(KexDir)) || !GetWindowsDirectory(Windows, ARRAYSIZE(Windows))) return FALSE;
    Error = MenuCommand(KexDir, Windows, 0, Command, ARRAYSIZE(Command), Legacy); if (Error) { SetLastError(Error); return FALSE; }
    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Classes\\exefile\\shell\\open_vxkex", 0, KEY_READ | KEY_WOW64_64KEY, &Menu);
    if (Error) { SetLastError(Error); return FALSE; }
    Error = OwnedMenu(Menu, Command, Legacy, NULL, &Owned);
    if (!Error && Owned) {
        Error = RegOpenKeyEx(Menu, L"command", 0, KEY_READ, &Key);
        if (!Error) {
            Error = StringValue(Key, NULL, Value, ARRAYSIZE(Value)); RegCloseKey(Key);
            if (!Error) Owned = !_wcsicmp(Value, Command) || !_wcsicmp(Value, Legacy);
        }
    }
    if (!Error && Owned && ExtendedMenu) *ExtendedMenu = !StringValue(Menu, L"Extended", Value, ARRAYSIZE(Value)) && !Value[0];
    RegCloseKey(Menu); if (Error) SetLastError(Error); return !Error && Owned;
}
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgConfigureShellContextMenuEntries(BOOLEAN Enable, BOOLEAN ExtendedMenu, HANDLE Transaction)
{
    WCHAR KexDir[MAX_PATH], Windows[MAX_PATH]; HKEY Classes; LONG Error; BOOL OwnTransaction = !Transaction;
    if (!KxCfgGetKexDir(KexDir, ARRAYSIZE(KexDir))) return FALSE;
    if (!GetWindowsDirectory(Windows, ARRAYSIZE(Windows))) return FALSE;
    if (OwnTransaction) { Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL); if (Transaction == INVALID_HANDLE_VALUE) return FALSE; }
    Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, L"Software\\Classes", 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Classes, Transaction, NULL);
    if (!Error) { Error = KxCfgpUpdateContextMenu(Classes, KexDir, Windows, Enable, ExtendedMenu, Transaction); RegCloseKey(Classes); }
    if (OwnTransaction) {
        if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
        if (Error && !RollbackTransaction(Transaction)) { DWORD RollbackError = GetLastError(); if (!Error) Error = RollbackError; }
        CloseHandle(Transaction);
    }
    if (Error) SetLastError(Error); return !Error;
}
