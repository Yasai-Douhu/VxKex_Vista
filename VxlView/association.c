#include "vxlview.h"
#include <ShlObj.h>
#define KXCFGDECLSPEC
#include <KxCfgHlp.h>

LONG VxlUpdateAssociation(HKEY ClassesRoot, PCWSTR Executable, BOOLEAN Install)
{
    HANDLE Transaction;
    NTSTATUS Status;
    LONG Error;
    Status = NtCreateTransaction(&Transaction, TRANSACTION_ALL_ACCESS, NULL,
        NULL, NULL, 0, 0, 0, NULL, NULL);
    if (!NT_SUCCESS(Status)) return RtlNtStatusToDosError(Status);
    Error = KxCfgpUpdateLogAssociation(ClassesRoot, Executable, Install, Transaction);
    if (!Error) {
        Status = NtCommitTransaction(Transaction, TRUE);
        if (!NT_SUCCESS(Status)) {
            Error = RtlNtStatusToDosError(Status);
            NtRollbackTransaction(Transaction, TRUE);
        }
    } else NtRollbackTransaction(Transaction, TRUE);
    NtClose(Transaction);
    return Error;
}
BOOLEAN VxlAssociationCommand(VOID)
{
    int ArgumentCount;
    PWSTR *Arguments = CommandLineToArgvW(GetCommandLine(), &ArgumentCount);
    BOOLEAN Install;
    WCHAR Executable[MAX_PATH];
    HKEY Classes;
    LONG Error;
    DWORD Length;
    if (!Arguments) ExitProcess(ERROR_NOT_ENOUGH_MEMORY);
    if (ArgumentCount == 2 && !_wcsicmp(Arguments[1], L"/register")) Install = TRUE;
    else if (ArgumentCount == 2 && !_wcsicmp(Arguments[1], L"/unregister")) Install = FALSE;
    else { LocalFree(Arguments); return FALSE; }
    LocalFree(Arguments);
    Length = GetModuleFileName(NULL, Executable, ARRAYSIZE(Executable));
    if (!Length || Length >= ARRAYSIZE(Executable)) ExitProcess(ERROR_BAD_PATHNAME);
    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Classes", 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Classes);
    if (!Error) {
        Error = VxlUpdateAssociation(Classes, Executable, Install);
        RegCloseKey(Classes);
    }
    if (!Error) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    ExitProcess(Error);
    return TRUE;
}
