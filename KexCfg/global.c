#include "kexcfg.h"
#include <ktmw32.h>
#include <sddl.h>
#include <stdio.h>

static LONG OpenSelectedUser(PCWSTR Text, HANDLE Transaction, PHKEY Key)
{
    PSID Sid; PWSTR Canonical = NULL; WCHAR Path[256]; LONG Error;
    SID_IDENTIFIER_AUTHORITY Authority = SECURITY_NT_AUTHORITY;
    if (!ConvertStringSidToSid(Text, &Sid)) return ERROR_INVALID_PARAMETER;
    if (!IsValidSid(Sid) || *GetSidSubAuthorityCount(Sid) != 5 ||
        memcmp(GetSidIdentifierAuthority(Sid), &Authority, sizeof(Authority)) ||
        *GetSidSubAuthority(Sid, 0) != SECURITY_NT_NON_UNIQUE) Error = ERROR_INVALID_PARAMETER;
    else if (!ConvertSidToStringSid(Sid, &Canonical)) Error = GetLastError();
    else if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Software", Canonical))) Error = ERROR_BAD_PATHNAME;
    else Error = RegOpenKeyTransacted(HKEY_USERS, Path, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, Key, Transaction, NULL);
    if (Canonical) LocalFree(Canonical); LocalFree(Sid); return Error;
}
DWORD KexCfgApplyGlobalArguments(int Count, PWSTR *Args)
{
    PCWSTR Sid = NULL, LogDir = NULL; DWORD Seen = 0, Fields[4] = {0}; int Index;
    HANDLE Transaction; HKEY User = NULL, Machine = NULL, Classes = NULL, Product = NULL;
    WCHAR KexDir[MAX_PATH], Windows[MAX_PATH], Msiexec[MAX_PATH]; DWORD Length, Error, RollbackError;
    PCWSTR Stage = L"validate"; BOOL CheckOnly = !_wcsicmp(Args[1], L"/CHECKGLOBAL");
#ifndef _WIN64
    return ERROR_NOT_SUPPORTED; // The installed native helper manages both physical MSI images.
#endif
    if (Count != 8) return ERROR_INVALID_PARAMETER;
    for (Index = 2; Index < Count; ++Index) {
        PCWSTR Arg = Args[Index], Value; DWORD Field; PWSTR End; ULONG Number;
        if (!_wcsnicmp(Arg, L"/USER-SID:", 10)) { Field = 0; Value = Arg + 10; }
        else if (!_wcsnicmp(Arg, L"/LOGDIR:", 8)) { Field = 1; Value = Arg + 8; }
        else if (!_wcsnicmp(Arg, L"/LOGGING:", 9)) { Field = 2; Value = Arg + 9; }
        else if (!_wcsnicmp(Arg, L"/MSI:", 5)) { Field = 3; Value = Arg + 5; }
        else if (!_wcsnicmp(Arg, L"/CONTEXTMENU:", 13)) { Field = 4; Value = Arg + 13; }
        else if (!_wcsnicmp(Arg, L"/EXTENDED:", 10)) { Field = 5; Value = Arg + 10; }
        else return ERROR_INVALID_PARAMETER;
        if ((Seen & (1 << Field)) || !Value[0]) return ERROR_INVALID_PARAMETER;
        Seen |= 1 << Field;
        if (!Field) { Sid = Value; continue; }
        if (Field == 1) { LogDir = Value; continue; }
        Number = wcstoul(Value, &End, 10);
        if (*End || *Value == L'-' || Number > 1) return ERROR_INVALID_PARAMETER;
        Fields[Field - 2] = Number;
    }
    if (Seen != 63 || !LogDir || PathIsRelative(LogDir) || wcslen(LogDir) >= MAX_PATH) return ERROR_INVALID_PARAMETER;
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Length = GetWindowsDirectory(Windows, ARRAYSIZE(Windows));
    if (!Length || Length >= ARRAYSIZE(Windows)) return ERROR_BAD_PATHNAME;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) return GetLastError();
    Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, L"Software", 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Machine, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(Machine, L"Classes", 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Classes, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(Machine, L"VXsoft\\VxKex", 0, KEY_READ, &Product, Transaction, NULL);
    if (!Error) Error = RegReadString(Product, NULL, L"KexDir", KexDir, ARRAYSIZE(KexDir));
    if (!Error) Error = OpenSelectedUser(Sid, Transaction, &User);
    if (!Error) {
        Stage = L"logging";
        Error = KxCfgpStageLoggingSettings(User, Machine, KexDir, (BOOLEAN)Fields[0], LogDir, Transaction);
    }
    if (!Error) Stage = L"msi";
    for (Index = 0; !Error && Index < 2; ++Index) {
        KXCFG_PROGRAM_CONFIGURATION Configuration = {0}; Configuration.Enabled = TRUE;
        if (FAILED(StringCchPrintf(Msiexec, ARRAYSIZE(Msiexec), L"%s\\%s\\msiexec.exe", Windows, Index ? L"SysWOW64" : L"System32"))) Error = ERROR_BAD_PATHNAME;
        else if (!(Fields[1] ? KxCfgSetConfiguration(Msiexec, &Configuration, Transaction) : KxCfgDeleteConfiguration(Msiexec, Transaction))) {
            Error = GetLastError(); if (!Error) Error = ERROR_GEN_FAILURE;
        }
    }
    if (!Error) {
        Stage = L"context-menu";
        Error = KxCfgpUpdateContextMenu(Classes, KexDir, Windows, (BOOLEAN)Fields[2], (BOOLEAN)Fields[3], Transaction);
    }
    if (User) RegCloseKey(User); if (Product) RegCloseKey(Product);
    if (Classes) RegCloseKey(Classes); if (Machine) RegCloseKey(Machine);
    if (!Error && !CheckOnly && !CommitTransaction(Transaction)) Error = GetLastError();
    if (Error || CheckOnly) {
        if (!RollbackTransaction(Transaction)) {
            RollbackError = GetLastError(); fwprintf(stderr, L"Global settings rollback failed: %lu.\n", RollbackError);
            if (!Error) Error = RollbackError;
        }
    }
    CloseHandle(Transaction);
    wprintf(L"Global settings check=%u stage=%s status=%lu\n", CheckOnly, Stage, Error);
    return Error;
}
