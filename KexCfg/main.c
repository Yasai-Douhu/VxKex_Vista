#include "buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <ktmw32.h>

// Only this small, explicitly elevated process writes configuration from Explorer.
static DWORD ApplyArguments(VOID)
{
    int Count, Index;
    PWSTR *Arguments = CommandLineToArgvW(GetCommandLineW(), &Count);
    PCWSTR Exe = NULL;
    KXCFG_PROGRAM_CONFIGURATION Config = {0};
    DWORD Error = ERROR_INVALID_PARAMETER;
    HANDLE Transaction;
    if (!Arguments) return GetLastError();
    for (Index = 1; Index < Count; ++Index) {
        PCWSTR Arg = Arguments[Index];
        PCWSTR Value;
        PWSTR End;
        ULONG Number;
        ULONG Field;
        if (!_wcsnicmp(Arg, L"/EXE:", 5)) {
            Exe = Arg + 5;
            if (!*Exe || wcslen(Exe) >= MAX_PATH || PathIsRelative(Exe)) goto Done;
            continue;
        }
        if (!_wcsnicmp(Arg, L"/ENABLE:", 8)) { Field = 0; Value = Arg + 8; }
        else if (!_wcsnicmp(Arg, L"/DISABLEFORCHILD:", 17)) { Field = 1; Value = Arg + 17; }
        else if (!_wcsnicmp(Arg, L"/DISABLEAPPSPECIFIC:", 20)) { Field = 2; Value = Arg + 20; }
        else if (!_wcsnicmp(Arg, L"/WINVERSPOOF:", 13)) { Field = 3; Value = Arg + 13; }
        else if (!_wcsnicmp(Arg, L"/STRONGSPOOF:", 13)) { Field = 4; Value = Arg + 13; }
        else goto Done;
        Number = wcstoul(Value, &End, Field == 4 ? 16 : 10);
        if (!*Value || *End || *Value == L'-') goto Done;
        if (Field < 3 && Number > 1) goto Done;
        if (Field == 3 && Number > WinVerSpoofWin11) goto Done;
        if (Field == 4 && (Number & ~KEX_STRONGSPOOF_VALID_MASK)) goto Done;
        switch (Field) {
        case 0: Config.Enabled = Number; break;
        case 1: Config.DisableForChild = Number; break;
        case 2: Config.DisableAppSpecificHacks = Number; break;
        case 3: Config.WinVerSpoof = Number; break;
        case 4: Config.StrongSpoofOptions = Number; break;
        }
    }
    if (!Exe) goto Done;
    // Never recurse into another elevation attempt if launched without elevation.
    if (KxCfgpElevationRequired()) { Error = ERROR_ACCESS_DENIED; goto Done; }
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) { Error = GetLastError(); goto Done; }
    if (!KxCfgSetConfiguration(Exe, &Config, Transaction)) {
        Error = GetLastError();
        if (!Error) Error = ERROR_GEN_FAILURE;
        RollbackTransaction(Transaction);
    } else if (!CommitTransaction(Transaction)) {
        Error = GetLastError();
    } else {
        Error = ERROR_SUCCESS;
    }
    CloseHandle(Transaction);
Done:
    LocalFree(Arguments);
    return Error;
}

VOID WINAPI KexCfgEntry(VOID)
{
    ExitProcess(ApplyArguments());
}
