#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupPackage-x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-package-x64.txt"
#else
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupPackage-x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-package-x86.txt"
#endif
#define PACKAGE ROOT L"\\Package"
#define TARGET ROOT L"\\Installed"
#define NATIVE ROOT L"\\Native"
#define WOW ROOT L"\\Wow"
static FILE *log; static unsigned failures;
static PCWSTR libraries[] = {L"KexDll.dll", L"KxBase.dll", L"KxNt.dll", L"KxAdvapi.dll",
    L"KxCom.dll", L"KxCrt.dll", L"KxCryp.dll", L"KxDw.dll", L"KxDx.dll", L"KxMi.dll",
    L"KxNet.dll", L"KxUia.dll", L"KxUser.dll"};
static PCWSTR extra[] = {L"KexShlEx.dll", L"VistaRun.exe", L"KexCfg.exe", L"VxlView.exe",
    L"install.bat", L"Remove-VxKex-Files.cmd", L"Kex64\\dwrw10.dll", L"VistaPty\\fixture.cmd"};
static void check(BOOL ok, PCSTR name)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", name, GetLastError());
    fflush(log); if (!ok) ++failures;
}
static BOOL put(PCWSTR path, PCSTR value)
{
    HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written, bytes = (DWORD)strlen(value); BOOL ok;
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = WriteFile(file, value, bytes, &written, NULL) && written == bytes;
    CloseHandle(file); return ok;
}
static BOOL get(PCWSTR path, PCSTR expected)
{
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, 0, NULL); DWORD bytes; CHAR value[64]; BOOL ok;
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, value, sizeof(value), &bytes, NULL) && bytes == strlen(expected) && !memcmp(value, expected, bytes);
    CloseHandle(file); return ok;
}
static HANDLE begin(void)
{
    HANDLE tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    check(tx != INVALID_HANDLE_VALUE, "create transaction"); return tx;
}
static void finish(HANDLE tx, BOOL commit)
{
    check(commit ? CommitTransaction(tx) : RollbackTransaction(tx), commit ? "commit" : "rollback");
    CloseHandle(tx);
}
static void packageLibrary(unsigned view, unsigned index, PCSTR bytes)
{
    WCHAR path[MAX_PATH];
    StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s%s", PACKAGE, view ? L"Kex32\\" : L"", libraries[index]);
    check(put(path, bytes), "write package library fixture");
}
static void verifyInstalled(void)
{
    WCHAR path[MAX_PATH]; unsigned index, view;
    for (view = 0; view < 2; ++view) for (index = 0; index < ARRAYSIZE(libraries); ++index) {
        StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s", view ? WOW : NATIVE, libraries[index]);
        check(get(path, view ? "wow" : "native"), "system library bytes");
        StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s%s", TARGET, view ? L"Kex32\\" : L"", libraries[index]);
        check(get(path, view ? "wow" : "native"), "installed package library bytes");
    }
    check(get(TARGET L"\\VistaPty\\fixture.cmd", "extra"), "bundled subtree copied");
    check(get(TARGET L"\\Kex64\\dwrw10.dll", "extra"), "Kex64 copied");
    check(get(NATIVE L"\\KxUnrelated.dll", "foreign-native") && get(WOW L"\\KxUnrelated.dll", "foreign-wow"), "unrelated system files unchanged");
}
void __cdecl mainCRTStartup(void)
{
    unsigned index, view; WCHAR path[MAX_PATH]; LONG error; HANDLE tx, lock;
    log = fopen(LOGFILE, "w"); if (!log) ExitProcess(2);
    if (GetFileAttributes(ROOT) != INVALID_FILE_ATTRIBUTES) { check(FALSE, "refuse existing fixture"); fclose(log); ExitProcess(3); }
    check(CreateDirectory(ROOT, NULL), "create fixture root");
    check(CreateDirectory(PACKAGE, NULL), "create package");
    check(CreateDirectory(PACKAGE L"\\Kex32", NULL), "create package Kex32");
    check(CreateDirectory(PACKAGE L"\\Kex64", NULL), "create package Kex64");
    check(CreateDirectory(PACKAGE L"\\VistaPty", NULL), "create package VistaPty");
    check(CreateDirectory(NATIVE, NULL), "create native fixture");
    check(CreateDirectory(WOW, NULL), "create WOW fixture");
    for (view = 0; view < 2; ++view) for (index = 0; index < ARRAYSIZE(libraries); ++index) packageLibrary(view, index, view ? "wow" : "native");
    for (index = 0; index < ARRAYSIZE(extra); ++index) {
        StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s", PACKAGE, extra[index]); check(put(path, "extra"), "write extra package fixture");
    }
    check(put(NATIVE L"\\KxUnrelated.dll", "foreign-native"), "write unrelated native file");
    check(put(WOW L"\\KxUnrelated.dll", "foreign-wow"), "write unrelated WOW file");
    if (failures) { fclose(log); ExitProcess(4); }
    check(DeleteFile(PACKAGE L"\\Kex32\\KxUser.dll"), "make incomplete package");
    tx = begin(); error = KxCfgpDeploySetupFiles(PACKAGE, TARGET, NATIVE, WOW, tx);
    check(error == ERROR_FILE_NOT_FOUND, "reject incomplete package before deployment"); finish(tx, FALSE);
    check(GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES && GetFileAttributes(NATIVE L"\\KexDll.dll") == INVALID_FILE_ATTRIBUTES, "incomplete package made no destinations");
    packageLibrary(1, ARRAYSIZE(libraries) - 1, "wow");
    check(put(PACKAGE L"\\KxSChanl.dll", "tls-native"), "write one TLS architecture");
    tx = begin(); check(KxCfgpDeploySetupFiles(PACKAGE, TARGET, NATIVE, WOW, tx) == ERROR_FILE_NOT_FOUND, "reject incomplete TLS pair"); finish(tx, FALSE);
    check(put(PACKAGE L"\\Kex32\\KxSChanl.dll", "tls-wow"), "write second TLS architecture");
    tx = begin(); error = KxCfgpDeploySetupFiles(PACKAGE, TARGET, NATIVE, WOW, tx);
    fprintf(log, "DeploymentStatus=%ld\n", error); check(!error, "stage complete deployment"); finish(tx, !error);
    verifyInstalled();
    check(get(NATIVE L"\\KxSChanl.dll", "tls-native") && get(WOW L"\\KxSChanl.dll", "tls-wow"), "deploy both TLS providers");

    packageLibrary(0, 0, "update-native"); packageLibrary(1, 0, "update-wow");
    lock = CreateFile(WOW L"\\KxUser.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "lock late WOW destination");
    tx = begin(); error = KxCfgpDeploySetupFiles(PACKAGE, TARGET, NATIVE, WOW, tx);
    fprintf(log, "LockedDeploymentStatus=%ld\n", error); check(error == ERROR_SHARING_VIOLATION, "late WOW copy fails"); finish(tx, FALSE);
    verifyInstalled();
    tx = begin(); error = KxCfgpRemoveSetupFiles(TARGET, NATIVE, WOW, tx);
    fprintf(log, "LockedRemovalStatus=%ld\n", error); check(error == ERROR_SHARING_VIOLATION, "late WOW removal fails"); finish(tx, FALSE);
    CloseHandle(lock); verifyInstalled();
    tx = begin(); error = KxCfgpRemoveSetupFiles(TARGET, NATIVE, WOW, tx);
    check(!error, "stage complete uninstall"); finish(tx, !error);
    check(GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES, "remove installation tree");
    check(GetFileAttributes(NATIVE L"\\KexDll.dll") == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributes(WOW L"\\KexDll.dll") == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributes(NATIVE L"\\KxSChanl.dll") == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributes(WOW L"\\KxSChanl.dll") == INVALID_FILE_ATTRIBUTES, "remove deployed providers");
    check(get(NATIVE L"\\KxUnrelated.dll", "foreign-native") && get(WOW L"\\KxUnrelated.dll", "foreign-wow"), "uninstall preserves unrelated libraries");
    tx = begin(); check(!KxCfgpRemoveSetupFiles(TARGET, NATIVE, WOW, tx), "repeated removal succeeds"); finish(tx, TRUE);
    tx = begin(); check(KxCfgpDeploySetupFiles(PACKAGE, TARGET, NATIVE, NATIVE, tx) == ERROR_INVALID_PARAMETER, "reject aliased system roots"); finish(tx, FALSE);
    tx = begin(); error = KxCfgpRemoveSetupDirectory(ROOT, tx); check(!error, "clean dedicated fixture"); finish(tx, !error);
    check(GetFileAttributes(ROOT) == INVALID_FILE_ATTRIBUTES, "fixture removed");
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
