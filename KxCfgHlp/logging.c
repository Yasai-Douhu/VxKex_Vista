#include "buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <ktmw32.h>

static LONG ReadLoggingLayer(HKEY Root, PBOOLEAN Enabled, PWSTR Path)
{
    HKEY Key; LONG Error; ULONG Value; WCHAR Override[MAX_PATH];
    Error = RegOpenKeyEx(Root, L"Software\\VXsoft\\VxKex", 0, KEY_READ | KEY_WOW64_64KEY, &Key);
    if (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = RegReadI32(Key, NULL, L"EnableLogging", &Value);
    if (!Error) *Enabled = !!Value;
    else if (Error != ERROR_FILE_NOT_FOUND) { RegCloseKey(Key); return Error; }
    Error = RegReadString(Key, NULL, L"LogDir", Override, ARRAYSIZE(Override));
    if (!Error) StringCchCopy(Path, MAX_PATH, Override);
    else if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
    RegCloseKey(Key); return Error;
}

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgQueryLoggingSettings(
    OUT PBOOLEAN IsEnabled OPTIONAL,
    OUT PWSTR LogDir OPTIONAL,
    IN ULONG LogDirCch)
{
    BOOLEAN Enabled = FALSE; WCHAR Path[MAX_PATH] = {0}; LONG Error;
    if (!!LogDir != !!LogDirCch) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (IsEnabled) *IsEnabled = FALSE;
    if (LogDir) LogDir[0] = 0;
    // Match KexDll's global configuration followed by per-user overrides.
    // Querying must never create a user key or require write permission.
    Error = ReadLoggingLayer(HKEY_LOCAL_MACHINE, &Enabled, Path);
    if (!Error) Error = ReadLoggingLayer(HKEY_CURRENT_USER, &Enabled, Path);
    if (!Error && LogDir && FAILED(StringCchCopy(LogDir, LogDirCch, Path))) Error = ERROR_MORE_DATA;
    if (Error) { if (LogDir) LogDir[0] = 0; SetLastError(Error); return FALSE; }
    if (IsEnabled) *IsEnabled = Enabled;
    return TRUE;
}
// The roots explicitly select the initiating user's Software hive and the
// machine Software hive. ResolvedLogDir must be expanded by the initiating
// process, never using a different elevated administrator's environment.
LONG KxCfgpStageLoggingSettings(HKEY UserSoftware, HKEY MachineSoftware,
    PCWSTR KexDir, BOOLEAN Enabled, PCWSTR ResolvedLogDir, HANDLE Transaction)
{
    HKEY User = NULL, Cleanup = NULL; LONG Error; WCHAR Icon[MAX_PATH + 32], Owner[80];
    PCWSTR Names[] = {NULL, L"Display", L"Description", L"Folder", L"FileList", L"IconPath"};
    PCWSTR Values[6]; unsigned Index;
    if (!UserSoftware || !MachineSoftware || !Transaction || Transaction == INVALID_HANDLE_VALUE ||
        Enabled > 1 || !KexDir || !ResolvedLogDir || !ResolvedLogDir[0] ||
        PathIsRelative(KexDir) || PathIsRelative(ResolvedLogDir) || wcslen(KexDir) >= MAX_PATH ||
        wcslen(ResolvedLogDir) >= MAX_PATH || wcschr(KexDir, L'"')) return ERROR_INVALID_PARAMETER;
    if (FAILED(StringCchPrintf(Icon, ARRAYSIZE(Icon), L"%s\\VxlView.exe,1", KexDir))) return ERROR_BAD_PATHNAME;
    Values[0] = L"{C0E13E61-0CC6-11d1-BBB6-0060978B2AE6}";
    Values[1] = L"VxKex Log Files";
    Values[2] = L"VxKex may create log files each time you launch an application, which consumes disk space. Log files older than 3 days can safely be deleted.";
    Values[3] = ResolvedLogDir; Values[4] = L"*.vxl"; Values[5] = Icon;
    Error = RegCreateKeyTransacted(MachineSoftware,
        L"Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files", 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &Cleanup, NULL, Transaction, NULL);
    if (Error) goto Done;
    Error = RegReadString(Cleanup, NULL, NULL, Owner, ARRAYSIZE(Owner));
    if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
    else if (!Error && _wcsicmp(Owner, Values[0])) Error = ERROR_ALREADY_EXISTS;
    if (Error) goto Done;
    Error = RegCreateKeyTransacted(UserSoftware, L"VXsoft\\VxKex", 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &User, NULL, Transaction, NULL);
    if (Error) goto Done;
    Error = RegWriteI32(User, NULL, L"EnableLogging", Enabled);
    if (!Error) Error = RegWriteString(User, NULL, L"LogDir", ResolvedLogDir);
    for (Index = 0; !Error && Index < ARRAYSIZE(Names); ++Index)
        Error = RegWriteString(Cleanup, NULL, Names[Index], Values[Index]);
    if (!Error) Error = RegWriteI32(Cleanup, NULL, L"LastAccess", 3);
    if (!Error) Error = RegWriteI32(Cleanup, NULL, L"Flags", 0x20);
Done:
    if (User) RegCloseKey(User); if (Cleanup) RegCloseKey(Cleanup); return Error;
}

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgConfigureLoggingSettings(
    BOOLEAN Enabled, PCWSTR LogDir, HANDLE Transaction)
{
    HKEY UserSoftware = NULL, MachineSoftware = NULL; WCHAR Expanded[MAX_PATH], KexDir[MAX_PATH];
    DWORD Length; LONG Error; BOOL OwnTransaction = !Transaction;
    if (!LogDir || !LogDir[0]) LogDir = L"%LOCALAPPDATA%\\VxKex\\Logs";
    Length = ExpandEnvironmentStrings(LogDir, Expanded, ARRAYSIZE(Expanded));
    if (!Length) return FALSE;
    if (Length > ARRAYSIZE(Expanded)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    if (!KxCfgGetKexDir(KexDir, ARRAYSIZE(KexDir))) return FALSE;
    if (OwnTransaction) {
        Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
        if (Transaction == INVALID_HANDLE_VALUE) return FALSE;
    }
    Error = RegCreateKeyTransacted(HKEY_CURRENT_USER, L"Software", 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &UserSoftware, NULL, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, L"Software", 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &MachineSoftware, Transaction, NULL);
    if (!Error) Error = KxCfgpStageLoggingSettings(UserSoftware, MachineSoftware, KexDir, Enabled, Expanded, Transaction);
    if (UserSoftware) RegCloseKey(UserSoftware); if (MachineSoftware) RegCloseKey(MachineSoftware);
    if (OwnTransaction) {
        if (!Error && !CommitTransaction(Transaction)) Error = GetLastError();
        if (Error) RollbackTransaction(Transaction);
        CloseHandle(Transaction);
    }
    if (Error) SetLastError(Error); return !Error;
}
