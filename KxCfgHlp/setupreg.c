#include "buildcfg.h"
#include <KxCfgHlp.h>

#define SETTINGS L"VXsoft\\VxKex"
#define LEGACY_SETTINGS L"VXsoft\\VxKexLdr"
#define TEMPLATE L"{VxKexPropagationVirtualKey}"

static LONG Text(HKEY Key, PCWSTR Name, PWSTR Buffer, DWORD Characters, DWORD *TypeOut)
{
    DWORD Type, Bytes = Characters * sizeof(WCHAR);
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Buffer, &Bytes);
    if (Error == ERROR_FILE_NOT_FOUND) { Buffer[0] = 0; if (TypeOut) *TypeOut = 0; return 0; }
    if (Error) return Error;
    if ((Type != REG_SZ && Type != REG_EXPAND_SZ) || Bytes < 2 || Bytes > Characters * 2 ||
        (Bytes & 1) || Buffer[Bytes / 2 - 1]) return ERROR_INVALID_DATA;
    if (TypeOut) *TypeOut = Type; return 0;
}
static LONG Dword(HKEY Key, PCWSTR Name, DWORD Default, PDWORD Value)
{
    DWORD Type, Bytes = sizeof(*Value);
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Value, &Bytes);
    if (Error == ERROR_FILE_NOT_FOUND) { *Value = Default; return 0; }
    if (Error == ERROR_MORE_DATA) return ERROR_INVALID_DATA;
    if (Error) return Error;
    return Type == REG_DWORD && Bytes == sizeof(*Value) ? 0 : ERROR_INVALID_DATA;
}
static LONG PutText(HKEY Key, PCWSTR Name, PCWSTR Value)
{
    return RegSetValueEx(Key, Name, 0, REG_SZ, (PCBYTE)Value, (DWORD)((wcslen(Value) + 1) * 2));
}
static LONG PutDword(HKEY Key, PCWSTR Name, DWORD Value)
{
    return RegSetValueEx(Key, Name, 0, REG_DWORD, (PCBYTE)&Value, sizeof(Value));
}
static LONG DeleteValue(HKEY Key, PCWSTR Name)
{
    LONG Error = RegDeleteValue(Key, Name); return Error == ERROR_FILE_NOT_FOUND ? 0 : Error;
}
static LONG DeleteProductKey(HKEY Root, PCWSTR Path, HANDLE Transaction)
{
    HKEY Key; LONG Error;
    if (!Root) return 0;
    Error = RegOpenKeyTransacted(Root, Path, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = RegDeleteTree(Key, NULL); RegCloseKey(Key);
    if (!Error) Error = RegDeleteKeyTransacted(Root, Path, KEY_WOW64_64KEY, 0, Transaction, NULL);
    return Error;
}

// UserSoftware may be NULL when a different user's preferences cannot be
// represented by the elevated token. Never infer the original user's hive.
LONG KxCfgpConfigureSetupSettings(HKEY MachineSoftware, HKEY UserSoftware,
    PCWSTR Target, DWORD Version, BOOLEAN Install, BOOLEAN KeepSettings, HANDLE Transaction)
{
    HKEY Key; WCHAR Current[MAX_PATH], LogDir[MAX_PATH]; DWORD Type; LONG Error;
    if (!MachineSoftware || !Transaction || Transaction == INVALID_HANDLE_VALUE ||
        !Target || !Target[0] || wcslen(Target) >= MAX_PATH || PathIsRelative(Target)) return ERROR_INVALID_PARAMETER;
    if (Install) Error = RegCreateKeyTransacted(MachineSoftware, SETTINGS, 0, NULL, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &Key, NULL, Transaction, NULL);
    else Error = RegOpenKeyTransacted(MachineSoftware, SETTINGS, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Key, Transaction, NULL);
    if (!Install && Error == ERROR_FILE_NOT_FOUND) Error = 0;
    else {
        if (Error) return Error;
        Error = Text(Key, L"KexDir", Current, ARRAYSIZE(Current), &Type);
        if (!Error && Type && (Type != REG_SZ || _wcsicmp(Current, Target))) Error = ERROR_ALREADY_EXISTS;
        if (!Error && Install) {
            Error = Text(Key, L"LogDir", LogDir, ARRAYSIZE(LogDir), &Type);
            if (!Error && !Type) {
                if (FAILED(StringCchPrintf(LogDir, ARRAYSIZE(LogDir), L"%s\\Logs", Target))) Error = ERROR_FILENAME_EXCED_RANGE;
                else Error = PutText(Key, L"LogDir", LogDir);
            }
            if (!Error) Error = PutText(Key, L"KexDir", Target);
            if (!Error) Error = PutDword(Key, L"InstalledVersion", Version);
        } else if (!Error && KeepSettings) {
            Error = DeleteValue(Key, L"InstalledVersion");
            if (!Error) Error = DeleteValue(Key, L"KexDir");
        }
        RegCloseKey(Key);
        if (Error) return Error;
    }
    if (!Install && !KeepSettings) {
        Error = DeleteProductKey(MachineSoftware, SETTINGS, Transaction);
        if (!Error) Error = DeleteProductKey(MachineSoftware, LEGACY_SETTINGS, Transaction);
        if (!Error) Error = DeleteProductKey(UserSoftware, SETTINGS, Transaction);
        if (!Error) Error = DeleteProductKey(UserSoftware, LEGACY_SETTINGS, Transaction);
    }
    return Error;
}

LONG KxCfgpConfigurePropagationTemplate(HKEY IfeoRoot, REGSAM View,
    BOOLEAN Install, HANDLE Transaction)
{
    HKEY Key; LONG Error; WCHAR Providers[256]; DWORD Flags, VerifierFlags, Type; SIZE_T Length;
    if (!IfeoRoot || !Transaction || Transaction == INVALID_HANDLE_VALUE ||
        (View != KEY_WOW64_64KEY && View != KEY_WOW64_32KEY)) return ERROR_INVALID_PARAMETER;
    if (Install) Error = RegCreateKeyTransacted(IfeoRoot, TEMPLATE, 0, NULL, 0,
        KEY_ALL_ACCESS | View, NULL, &Key, NULL, Transaction, NULL);
    else Error = RegOpenKeyTransacted(IfeoRoot, TEMPLATE, 0, KEY_ALL_ACCESS | View, &Key, Transaction, NULL);
    if (!Install && Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    if (!Install) Error = KxCfgpPreserveIfeoConfiguration(Key, NULL);
    else {
        Error = Text(Key, L"VerifierDlls", Providers, ARRAYSIZE(Providers), &Type);
        if (!Error && Type && Type != REG_SZ) Error = ERROR_INVALID_DATA;
        if (!Error) Error = Dword(Key, L"GlobalFlag", 0, &Flags);
        if (!Error) Error = Dword(Key, L"VerifierFlags", 0, &VerifierFlags);
        if (!Error) {
            KxCfgpRemoveKexDllFromVerifierDlls(Providers);
            if (wcsspn(Providers, L" \t") == wcslen(Providers)) Providers[0] = 0;
            // Foreign providers retain their verifier options. Kex-only defaults
            // avoid enabling Vista's unrelated application-verifier checks.
            if (!Providers[0]) {
                if (!VerifierFlags) VerifierFlags = 0x80000000;
                Error = PutText(Key, L"VerifierDlls", L"KexDll.dll");
            } else {
                HRESULT Result;
                Length = wcslen(Providers);
                Result = StringCchCat(Providers, ARRAYSIZE(Providers),
                    Providers[Length - 1] == L' ' || Providers[Length - 1] == L'\t' ? L"KexDll.dll" : L" KexDll.dll");
                Error = FAILED(Result) ? ERROR_MORE_DATA : PutText(Key, L"VerifierDlls", Providers);
            }
            if (!Error) Error = PutDword(Key, L"GlobalFlag", Flags | FLG_APPLICATION_VERIFIER);
            if (!Error) Error = PutDword(Key, L"VerifierFlags", VerifierFlags);
        }
    }
    RegCloseKey(Key); return Error;
}
