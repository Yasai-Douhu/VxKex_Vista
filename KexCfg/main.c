#include "buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include "kexcfg.h"

static BOOLEAN CALLBACK WriteConfigurationEntry(PCWSTR Path, BOOLEAN BasenameOnly, PVOID Output)
{
    WCHAR Line[MAX_PATH + 32];
    DWORD Written, Bytes;
    if (FAILED(StringCchPrintf(Line, ARRAYSIZE(Line), L"%s\t%s\r\n",
        BasenameOnly ? L"basename" : L"path", Path))) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    Bytes = (DWORD)wcslen(Line) * sizeof(WCHAR);
    if (!WriteFile((HANDLE)Output, Line, Bytes, &Written, NULL)) return FALSE;
    if (Written != Bytes) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return TRUE;
}

static DWORD ListConfiguration(PCWSTR OutputPath)
{
    HANDLE Output;
    WCHAR Bom = 0xfeff;
    DWORD Written, Error = ERROR_SUCCESS;
    if (!*OutputPath || PathIsRelative(OutputPath)) return ERROR_INVALID_PARAMETER;
    Output = CreateFileW(OutputPath, GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (Output == INVALID_HANDLE_VALUE) return GetLastError();
    if (!WriteFile(Output, &Bom, sizeof(Bom), &Written, NULL) || Written != sizeof(Bom))
        Error = ERROR_WRITE_FAULT;
    else if (!KxCfgEnumerateConfiguration(WriteConfigurationEntry, Output)) {
        Error = GetLastError();
        if (!Error) Error = ERROR_GEN_FAILURE;
    }
    CloseHandle(Output);
    return Error;
}

static DWORD PreserveConfiguration(DWORD Action, BOOLEAN CheckOnly)
{
    HANDLE Transaction;
    DWORD Error = 0;
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) return GetLastError();
    if (!(Action == 1 ? KxCfgRestoreAllConfigurations(Transaction) : Action >= 2 ?
        KxCfgPrepareUninstall(Action == 2, Transaction) :
        KxCfgPreserveAllConfigurations(Transaction))) Error = GetLastError();
    if (!Error && CheckOnly && Action != 3 && !KxCfgRestoreAllConfigurations(Transaction)) Error = GetLastError();
    if (Error || CheckOnly) {
        if (!RollbackTransaction(Transaction) && !Error) Error = GetLastError();
    } else if (!CommitTransaction(Transaction)) {
        Error = GetLastError();
        RollbackTransaction(Transaction);
    }
    CloseHandle(Transaction);
    return Error;
}

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
    if (Count == 1) {
        KexCfgOpenGUI();
        Error = ERROR_SUCCESS;
        goto Done;
    }
    if (!_wcsicmp(Arguments[1], L"/GLOBAL") || !_wcsicmp(Arguments[1], L"/CHECKGLOBAL")) {
        Error = KexCfgApplyGlobalArguments(Count, Arguments); goto Done;
    }
    if (!_wcsicmp(Arguments[1], L"/ADD") || !_wcsicmp(Arguments[1], L"/DELETE") ||
        !_wcsicmp(Arguments[1], L"/CLEAN")) {
        Error = KexCfgApplyBulkArguments(Count, Arguments); goto Done;
    }
    if (Count == 2 && !_wcsnicmp(Arguments[1], L"/LIST:", 6)) {
        Error = ListConfiguration(Arguments[1] + 6);
        goto Done;
    }
    if (Count == 2 && (!_wcsicmp(Arguments[1], L"/PRESERVE") ||
        !_wcsicmp(Arguments[1], L"/RESTORE") || !_wcsicmp(Arguments[1], L"/CHECKPRESERVE"))) {
        Error = PreserveConfiguration(!_wcsicmp(Arguments[1], L"/RESTORE") ? 1 : 0,
            !_wcsicmp(Arguments[1], L"/CHECKPRESERVE"));
        goto Done;
    }
    if (Count == 2 && (!_wcsicmp(Arguments[1], L"/UNINSTALLKEEP") ||
        !_wcsicmp(Arguments[1], L"/REMOVEALL") || !_wcsicmp(Arguments[1], L"/CHECKUNINSTALLKEEP") ||
        !_wcsicmp(Arguments[1], L"/CHECKREMOVEALL"))) {
        DWORD Action = (!_wcsicmp(Arguments[1], L"/UNINSTALLKEEP") ||
            !_wcsicmp(Arguments[1], L"/CHECKUNINSTALLKEEP")) ? 2 : 3;
        Error = PreserveConfiguration(Action, !_wcsnicmp(Arguments[1], L"/CHECK", 6));
        goto Done;
    }
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
