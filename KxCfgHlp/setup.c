#include "buildcfg.h"
#include <KxCfgHlp.h>

#define STORE_PATH L"VXsoft\\VxKexVistaPreserved"

static LONG DiscardStore(HKEY Software, HANDLE Transaction)
{
    HKEY Store; DWORD Version, Type, Bytes = sizeof(Version); LONG Error;
    Error = RegOpenKeyTransacted(Software, STORE_PATH, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Store, Transaction, NULL);
    if (Error == ERROR_FILE_NOT_FOUND) return 0;
    if (Error) return Error;
    Error = RegQueryValueEx(Store, L"StoreVersion", NULL, &Type, (PBYTE)&Version, &Bytes);
    if (!Error && (Type != REG_DWORD || Bytes != sizeof(Version) || Version != 1)) Error = ERROR_INVALID_DATA;
    if (!Error) Error = RegDeleteTree(Store, NULL);
    RegCloseKey(Store);
    if (!Error) Error = RegDeleteKeyTransacted(Software, STORE_PATH, KEY_WOW64_64KEY, 0, Transaction, NULL);
    return Error;
}
static LONG EnsureLogs(PCWSTR Target, HANDLE Transaction)
{
    WCHAR Path[MAX_PATH]; WIN32_FILE_ATTRIBUTE_DATA Data; LONG Error;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Logs", Target))) return ERROR_FILENAME_EXCED_RANGE;
    if (CreateDirectoryTransacted(NULL, Path, NULL, Transaction)) return 0;
    Error = GetLastError(); if (Error != ERROR_ALREADY_EXISTS) return Error;
    if (!GetFileAttributesTransacted(Path, GetFileExInfoStandard, &Data, Transaction)) return GetLastError();
    if (!(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return ERROR_INVALID_DATA;
    return 0;
}

LONG KxCfgpStageSetup(PKXCFG_SETUP_CONTEXT Context, BOOLEAN Install,
    BOOLEAN KeepSettings, HANDLE Transaction)
{
    HKEY Native = NULL, Wow = NULL; LONG Error;
    WCHAR ShellPath[MAX_PATH], ViewerPath[MAX_PATH];
    if (!Context || Context->Size != sizeof(*Context) || !Transaction || Transaction == INVALID_HANDLE_VALUE ||
        !Context->Target || !Context->NativeSystem || !Context->WowSystem || !Context->MachineSoftware ||
        !Context->ClassesRoot || !Context->NativeIfeo || !Context->WowIfeo ||
        (Install && (!Context->Package || !Context->InstalledVersion))) return ERROR_INVALID_PARAMETER;
    if (RtlOperatingSystemBitness() != 64) return ERROR_NOT_SUPPORTED;
    Context->StageName = L"open-ifeo-roots";
    if (FAILED(StringCchPrintf(ShellPath, ARRAYSIZE(ShellPath), L"%s\\KexShlEx.dll", Context->Target)) ||
        FAILED(StringCchPrintf(ViewerPath, ARRAYSIZE(ViewerPath), L"%s\\VxlView.exe", Context->Target))) return ERROR_FILENAME_EXCED_RANGE;
    // Reopen the caller's roots in this transaction so enumeration observes
    // staged changes. The caller still owns the original handles.
    Error = RegOpenKeyTransacted(Context->NativeIfeo, L"", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Native, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(Context->WowIfeo, L"", 0, KEY_ALL_ACCESS | KEY_WOW64_32KEY, &Wow, Transaction, NULL);
    if (Error) goto Done;
    if (Install) {
        Context->StageName = L"deploy-package";
        Error = KxCfgpDeploySetupPackage(Context->Package, Context->Target, Context->NativeSystem, Context->WowSystem, Transaction);
        if (!Error) Context->StageName = L"create-log-directory";
        if (!Error) Error = EnsureLogs(Context->Target, Transaction);
        if (!Error) Context->StageName = L"configure-global-settings";
        if (!Error) Error = KxCfgpConfigureSetupSettings(Context->MachineSoftware, Context->UserSoftware,
            Context->Target, Context->InstalledVersion, TRUE, TRUE, Transaction);
        if (!Error) Context->StageName = L"restore-profiles";
        if (!Error) Error = KxCfgpProcessConfigurationRoots(TRUE, Transaction, Context->MachineSoftware, STORE_PATH, Native, Wow);
    } else if (KeepSettings) {
        Context->StageName = L"preserve-profiles";
        Error = KxCfgpProcessConfigurationRoots(FALSE, Transaction, Context->MachineSoftware, STORE_PATH, Native, Wow);
    } else {
        Context->StageName = L"disable-profiles";
        Error = KxCfgpPreserveIfeoView(Native, NULL, Transaction, KEY_WOW64_64KEY);
        if (!Error) Error = KxCfgpPreserveIfeoView(Wow, NULL, Transaction, KEY_WOW64_32KEY);
        if (!Error) Context->StageName = L"discard-preserved-store";
        if (!Error) Error = DiscardStore(Context->MachineSoftware, Transaction);
    }
    if (!Error) Context->StageName = L"configure-propagation-template";
    if (!Error) Error = KxCfgpConfigurePropagationTemplate(Native, KEY_WOW64_64KEY, Install, Transaction);
    if (!Error) Error = KxCfgpConfigurePropagationTemplate(Wow, KEY_WOW64_32KEY, Install, Transaction);
    if (!Error) Context->StageName = L"configure-shell-extension";
    if (!Error) Error = KxCfgpUpdateShellExtension(Context->ClassesRoot, Context->MachineSoftware, ShellPath, Install, Transaction);
    if (!Error) Context->StageName = L"configure-log-association";
    if (!Error) Error = KxCfgpUpdateLogAssociation(Context->ClassesRoot, ViewerPath, Install, Transaction);
    if (!Error && !Install) Context->StageName = L"remove-global-settings";
    if (!Error && !Install) Error = KxCfgpConfigureSetupSettings(Context->MachineSoftware, Context->UserSoftware,
        Context->Target, 0, FALSE, KeepSettings, Transaction);
    // Disable all managed launch commands before staging removal of their files.
    if (!Error && !Install) Context->StageName = L"remove-files";
    if (!Error && !Install) Error = KxCfgpRemoveSetupFiles(Context->Target, Context->NativeSystem, Context->WowSystem, Transaction);
Done:
    if (Native) RegCloseKey(Native);
    if (Wow) RegCloseKey(Wow);
    return Error;
}
