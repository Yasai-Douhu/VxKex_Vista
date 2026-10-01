#include "buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <sddl.h>
#include <ShlObj.h>
#include <stdio.h>

#define IFEO L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"
#define TARGET L"C:\\VxKex"
DWORD VistaSetupStagePackage(PCWSTR Package, PCWSTR CacheRoot, PWSTR Staged, DWORD StagedChars);
DWORD VistaSetupCollectCache(PCWSTR CacheRoot, PCWSTR KeepPackage);
DWORD VistaSetupCompleteCache(PCWSTR CacheRoot, PCWSTR Package, HANDLE OwnerProcess);

static DWORD CompleteCache(void)
{
    WCHAR Common[MAX_PATH], Cache[MAX_PATH], Package[MAX_PATH]; DWORD Length, Error; HANDLE Parent;
    PROCESS_BASIC_INFORMATION Basic; NTSTATUS Status; HRESULT Result;
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Result = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, Common);
    if (FAILED(Result)) return HRESULT_CODE(Result);
    if (FAILED(StringCchPrintf(Cache, ARRAYSIZE(Cache), L"%s\\VxKexVistaSetup", Common))) return ERROR_BAD_PATHNAME;
    Length = GetModuleFileName(NULL, Package, ARRAYSIZE(Package));
    if (!Length || Length >= ARRAYSIZE(Package) || !PathRemoveFileSpec(Package)) return ERROR_BAD_PATHNAME;
    Status = NtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation, &Basic, sizeof(Basic), NULL);
    if (!NT_SUCCESS(Status)) return RtlNtStatusToDosError(Status);
    Parent = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)Basic.InheritedFromUniqueProcessId);
    if (!Parent) return GetLastError();
    Error = VistaSetupCompleteCache(Cache, Package, Parent); CloseHandle(Parent); return Error;
}

static DWORD PrepareCache(PCWSTR Package)
{
    RTL_OSVERSIONINFOEXW Version = {sizeof(Version)};
    WCHAR Common[MAX_PATH], Cache[MAX_PATH], Staged[MAX_PATH]; DWORD Error; HRESULT Result;
    NTSTATUS Status = RtlGetVersion(&Version);
    if (!NT_SUCCESS(Status)) return RtlNtStatusToDosError(Status);
    if (Version.dwMajorVersion != 6 || Version.dwMinorVersion != 0 || RtlOperatingSystemBitness() != 64) return ERROR_OLD_WIN_VERSION;
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Result = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, Common);
    if (FAILED(Result)) return HRESULT_CODE(Result);
    if (FAILED(StringCchPrintf(Cache, ARRAYSIZE(Cache), L"%s\\VxKexVistaSetup", Common))) return ERROR_BAD_PATHNAME;
    Error = VistaSetupStagePackage(Package, Cache, Staged, ARRAYSIZE(Staged));
    if (!Error) {
        DWORD CollectionError = VistaSetupCollectCache(Cache, Staged);
        if (CollectionError) fwprintf(stderr, L"Cache collection deferred: %lu.\n", CollectionError);
        wprintf(L"%s\n", Staged);
    }
    return Error;
}

static DWORD CurrentSid(PWSTR *Sid)
{
    HANDLE Token; DWORD Bytes = 0, Error = 0; PTOKEN_USER User = NULL;
    *Sid = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token)) return GetLastError();
    GetTokenInformation(Token, TokenUser, NULL, 0, &Bytes);
    User = HeapAlloc(GetProcessHeap(), 0, Bytes);
    if (!User) Error = ERROR_NOT_ENOUGH_MEMORY;
    else if (!GetTokenInformation(Token, TokenUser, User, Bytes, &Bytes) ||
        !ConvertSidToStringSid(User->User.Sid, Sid)) Error = GetLastError();
    if (User) HeapFree(GetProcessHeap(), 0, User);
    CloseHandle(Token); return Error;
}
static DWORD PrintCurrentSid(void)
{
    PWSTR Sid; DWORD Error = CurrentSid(&Sid);
    if (!Error) wprintf(L"%s\n", Sid);
    if (Sid) LocalFree(Sid); return Error;
}
static DWORD ElevateInstaller(void)
{
    WCHAR Batch[MAX_PATH], Parameters[256]; PWSTR Sid = NULL; DWORD Error, Length;
    SHELLEXECUTEINFO Info = {sizeof(Info)};
    Length = GetModuleFileName(NULL, Batch, ARRAYSIZE(Batch));
    if (!Length || Length >= ARRAYSIZE(Batch) || !PathRemoveFileSpec(Batch) ||
        FAILED(StringCchCat(Batch, ARRAYSIZE(Batch), L"\\install.bat"))) return ERROR_BAD_PATHNAME;
    Error = CurrentSid(&Sid); if (Error) return Error;
    if (FAILED(StringCchPrintf(Parameters, ARRAYSIZE(Parameters), L"--original-user-sid %s", Sid))) Error = ERROR_BAD_PATHNAME;
    else {
        Info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC; Info.lpVerb = L"runas";
        Info.lpFile = Batch; Info.lpParameters = Parameters; Info.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteEx(&Info)) Error = GetLastError();
        else if (Info.hProcess) CloseHandle(Info.hProcess);
    }
    LocalFree(Sid); return Error;
}
static DWORD OpenUserSoftware(PCWSTR SidText, HANDLE Transaction, PHKEY Key)
{
    PSID Sid; WCHAR Path[256]; SID_IDENTIFIER_AUTHORITY Authority = SECURITY_NT_AUTHORITY; DWORD Error;
    if (!ConvertStringSidToSid(SidText, &Sid)) return ERROR_INVALID_PARAMETER;
    if (!IsValidSid(Sid) || *GetSidSubAuthorityCount(Sid) != 5 ||
        memcmp(GetSidIdentifierAuthority(Sid), &Authority, sizeof(Authority)) ||
        *GetSidSubAuthority(Sid, 0) != SECURITY_NT_NON_UNIQUE) Error = ERROR_INVALID_PARAMETER;
    else if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\Software", SidText))) Error = ERROR_INVALID_PARAMETER;
    else Error = RegOpenKeyTransacted(HKEY_USERS, Path, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, Key, Transaction, NULL);
    LocalFree(Sid); return Error;
}
static DWORD CheckPackageVersion(PCWSTR Package)
{
    WCHAR Path[MAX_PATH]; DWORD Bytes, Error = 0, Fields[] = {KEX_VERSION_FV};
    PVOID Buffer; VS_FIXEDFILEINFO *Version; UINT Length;
    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\KexCfg.exe", Package))) return ERROR_FILENAME_EXCED_RANGE;
    Bytes = GetFileVersionInfoSize(Path, NULL); if (!Bytes) return GetLastError();
    Buffer = HeapAlloc(GetProcessHeap(), 0, Bytes); if (!Buffer) return ERROR_NOT_ENOUGH_MEMORY;
    if (!GetFileVersionInfo(Path, 0, Bytes, Buffer)) Error = GetLastError();
    else if (!VerQueryValue(Buffer, L"\\", (PVOID *)&Version, &Length) || Length < sizeof(*Version) ||
        Version->dwSignature != VS_FFI_SIGNATURE || Version->dwFileVersionMS != MAKELONG(Fields[1], Fields[0]) ||
        Version->dwFileVersionLS != MAKELONG(Fields[3], Fields[2])) Error = ERROR_REVISION_MISMATCH;
    HeapFree(GetProcessHeap(), 0, Buffer); return Error;
}
static DWORD Execute(PCWSTR Package, PCWSTR UserSid, BOOL Install, BOOL Keep, BOOL CheckOnly)
{
    KXCFG_SETUP_CONTEXT Context = {sizeof(Context)};
    RTL_OSVERSIONINFOEXW Version = {sizeof(Version)};
    WCHAR Windows[MAX_PATH], Native[MAX_PATH], Wow[MAX_PATH], Executable[MAX_PATH];
    DWORD Length, Error = 0, RollbackError; HANDLE Transaction;
    NTSTATUS Status; unsigned Index; HKEY Keys[5];
#ifndef _WIN64
    return ERROR_NOT_SUPPORTED;
#endif
    Status = RtlGetVersion(&Version);
    if (!NT_SUCCESS(Status)) return RtlNtStatusToDosError(Status);
    if (Version.dwMajorVersion != 6 || Version.dwMinorVersion != 0 || RtlOperatingSystemBitness() != 64) return ERROR_OLD_WIN_VERSION;
    if (KxCfgpElevationRequired()) return ERROR_ACCESS_DENIED;
    Length = GetModuleFileName(NULL, Executable, ARRAYSIZE(Executable));
    if (!Length || Length >= ARRAYSIZE(Executable)) return ERROR_BAD_PATHNAME;
    if (!_wcsnicmp(Executable, TARGET, wcslen(TARGET)) && Executable[wcslen(TARGET)] == L'\\') {
        fwprintf(stderr, L"Run the staged helper outside %s.\n", TARGET); return ERROR_ACCESS_DENIED;
    }
    Length = GetWindowsDirectory(Windows, ARRAYSIZE(Windows));
    if (!Length || Length >= ARRAYSIZE(Windows)) return ERROR_BAD_PATHNAME;
    Length = GetSystemDirectory(Native, ARRAYSIZE(Native));
    if (!Length || Length >= ARRAYSIZE(Native) ||
        FAILED(StringCchPrintf(Wow, ARRAYSIZE(Wow), L"%s\\SysWOW64", Windows))) return ERROR_BAD_PATHNAME;
    if (!SetCurrentDirectory(Windows)) return GetLastError();
    if (Install) { Error = CheckPackageVersion(Package); if (Error) return Error; }
    if (!Install && !Keep && !UserSid) return ERROR_INVALID_PARAMETER;
    Transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (Transaction == INVALID_HANDLE_VALUE) return GetLastError();
    Context.Package = Package; Context.Target = TARGET; Context.NativeSystem = Native; Context.WowSystem = Wow;
    Context.InstalledVersion = KEX_VERSION_DW;
    Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, L"Software", 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Context.MachineSoftware, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, L"Software\\Classes", 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Context.ClassesRoot, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Context.NativeIfeo, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO, 0,
        KEY_ALL_ACCESS | KEY_WOW64_32KEY, &Context.WowIfeo, Transaction, NULL);
    if (!Error && UserSid) Error = OpenUserSoftware(UserSid, Transaction, &Context.UserSoftware);
    if (!Error) Error = KxCfgpStageSetup(&Context, Install, Keep, Transaction);
    Keys[0] = Context.MachineSoftware; Keys[1] = Context.ClassesRoot; Keys[2] = Context.NativeIfeo;
    Keys[3] = Context.WowIfeo; Keys[4] = Context.UserSoftware;
    for (Index = 0; Index < ARRAYSIZE(Keys); ++Index) if (Keys[Index]) RegCloseKey(Keys[Index]);
    if (!Error && !CheckOnly) {
        if (!CommitTransaction(Transaction)) Error = GetLastError();
        else SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    if (Error || CheckOnly) {
        if (!RollbackTransaction(Transaction)) {
            RollbackError = GetLastError();
            fwprintf(stderr, L"Rollback failed: %lu.\n", RollbackError);
            if (!Error) Error = RollbackError;
        }
    }
    CloseHandle(Transaction);
    wprintf(L"Setup mode=%s check=%u stage=%s status=%lu\n", Install ? L"install" : Keep ? L"uninstall-keep" : L"uninstall-remove", CheckOnly,
        Context.StageName ? Context.StageName : L"initialize", Error);
    return Error;
}
VOID __cdecl mainCRTStartup(VOID)
{
    int Count, Index; PWSTR *Args = CommandLineToArgvW(GetCommandLine(), &Count);
    PCWSTR Package = NULL, UserSid = NULL; BOOL Install = FALSE, Keep = TRUE, CheckOnly = FALSE;
    DWORD Error = ERROR_INVALID_PARAMETER;
    if (!Args) ExitProcess(GetLastError());
    if (Count == 2 && !_wcsicmp(Args[1], L"--current-user-sid")) { Error = PrintCurrentSid(); goto Done; }
    if (Count == 2 && !_wcsicmp(Args[1], L"--is-elevated")) { Error = KxCfgpElevationRequired() ? ERROR_ACCESS_DENIED : 0; goto Done; }
    if (Count == 2 && !_wcsicmp(Args[1], L"--elevate-installer")) { Error = ElevateInstaller(); goto Done; }
    if (Count == 2 && !_wcsicmp(Args[1], L"--complete-cache")) { Error = CompleteCache(); goto Done; }
    if (Count == 3 && !_wcsicmp(Args[1], L"--prepare-cache")) {
        Error = CheckPackageVersion(Args[2]);
        if (!Error) Error = PrepareCache(Args[2]);
        goto Done;
    }
    if (Count < 2) goto Usage;
    if (!_wcsicmp(Args[1], L"--install") || !_wcsicmp(Args[1], L"--check-install")) {
        if (Count < 3 || PathIsRelative(Args[2]) || !Args[2][0]) goto Usage;
        Install = TRUE; CheckOnly = !_wcsicmp(Args[1], L"--check-install"); Package = Args[2]; Index = 3;
    } else {
        Index = 2;
        if (!_wcsicmp(Args[1], L"--uninstall-keep")) Keep = TRUE;
        else if (!_wcsicmp(Args[1], L"--uninstall-remove")) Keep = FALSE;
        else if (!_wcsicmp(Args[1], L"--check-uninstall-keep")) { Keep = TRUE; CheckOnly = TRUE; }
        else if (!_wcsicmp(Args[1], L"--check-uninstall-remove")) { Keep = FALSE; CheckOnly = TRUE; }
        else goto Usage;
    }
    if (Index < Count) {
        if (Index + 2 != Count || _wcsicmp(Args[Index], L"--user-sid")) goto Usage;
        UserSid = Args[Index + 1];
    }
    Error = Execute(Package, UserSid, Install, Keep, CheckOnly); goto Done;
Usage:
    fwprintf(stderr, L"VistaSetup --install|--check-install <absolute-package-path> [--user-sid SID]\n"
        L"VistaSetup --uninstall-keep|--check-uninstall-keep [--user-sid SID]\n"
        L"VistaSetup --uninstall-remove|--check-uninstall-remove --user-sid SID\n"
        L"VistaSetup --current-user-sid|--is-elevated|--elevate-installer\n"
        L"VistaSetup --prepare-cache <absolute-package-path>|--complete-cache\n");
Done:
    if (Error) fwprintf(stderr, L"VistaSetup failed: %lu.\n", Error);
    LocalFree(Args); ExitProcess(Error);
}
