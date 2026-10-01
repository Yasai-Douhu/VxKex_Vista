#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define ROOT L"Software\\VxKexSetupStage-x64"
#define FILEPATH L"C:\\VxKexProbe\\NextParity\\SetupRegistryFile-x64.txt"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-stage-x64.txt"
#else
#define ROOT L"Software\\VxKexSetupStage-x86"
#define FILEPATH L"C:\\VxKexProbe\\NextParity\\SetupRegistryFile-x86.txt"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-stage-x86.txt"
#endif
#ifdef _WIN64
#define TREE L"C:\\VxKexProbe\\NextParity\\SetupStage-x64"
#else
#define TREE L"C:\\VxKexProbe\\NextParity\\SetupStage-x86"
#endif
#define TARGET TREE L"\\Installed"
#define PRODUCT L"VXsoft\\VxKex"
#define TEMPLATE L"{VxKexPropagationVirtualKey}"
static HKEY root, machine, user, ifeo64, ifeo32;
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR name)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", name, GetLastError());
    fflush(log); if (!ok) ++failures;
}
static HKEY child(HKEY parent, PCWSTR path)
{
    HKEY key = NULL;
    check(!RegCreateKeyEx(parent, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL), "create fixture key"); return key;
}
static void putText(HKEY parent, PCWSTR path, PCWSTR name, PCWSTR value)
{
    HKEY key = child(parent, path);
    check(!RegSetValueEx(key, name, 0, REG_SZ, (PCBYTE)value, (DWORD)((wcslen(value) + 1) * 2)), "write fixture text"); RegCloseKey(key);
}
static void putDword(HKEY parent, PCWSTR path, PCWSTR name, DWORD value)
{
    HKEY key = child(parent, path);
    check(!RegSetValueEx(key, name, 0, REG_DWORD, (PCBYTE)&value, 4), "write fixture DWORD"); RegCloseKey(key);
}
static BOOL has(HKEY parent, PCWSTR path, PCWSTR name, DWORD type, PCVOID expected, DWORD length)
{
    BYTE data[1024]; DWORD bytes = sizeof(data), actualType; HKEY key;
    LONG error = RegOpenKeyEx(parent, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (error) return !expected && error == ERROR_FILE_NOT_FOUND;
    error = RegQueryValueEx(key, name, NULL, &actualType, data, &bytes); RegCloseKey(key);
    if (!expected) return error == ERROR_FILE_NOT_FOUND;
    return !error && actualType == type && bytes == length && !memcmp(data, expected, length);
}
static BOOL text(HKEY parent, PCWSTR path, PCWSTR name, PCWSTR value)
{
    return has(parent, path, name, REG_SZ, value, value ? (DWORD)((wcslen(value) + 1) * 2) : 0);
}
static BOOL dword(HKEY parent, PCWSTR path, PCWSTR name, DWORD value)
{
    return has(parent, path, name, REG_DWORD, &value, sizeof(value));
}
static HANDLE begin(void)
{
    HANDLE tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    check(tx != INVALID_HANDLE_VALUE, "create transaction"); return tx;
}
static void finish(HANDLE tx, BOOL commit)
{
    check(commit ? CommitTransaction(tx) : RollbackTransaction(tx), commit ? "commit" : "rollback"); CloseHandle(tx);
}
static KXCFG_SETUP_CONTEXT context = {sizeof(context)};
static LONG setup(BOOL install, BOOL keep, BOOL commit)
{
    HANDLE tx = begin(); LONG error = KxCfgpStageSetup(&context, install, keep, tx);
    fprintf(log, "StageSetup install=%d keep=%d status=%ld\n", install, keep, error);
    finish(tx, commit && !error); return error;
}
static void profile(HKEY ifeo, DWORD spoof, BOOL enabled)
{
    putDword(ifeo, L"fixture.exe", L"KEX_WinVerSpoof", spoof);
    putDword(ifeo, L"fixture.exe", L"GlobalFlag", enabled ? 0x102 : 2);
    if (enabled) {
        putText(ifeo, L"fixture.exe", L"VerifierDlls", L"Foreign.dll KexDll.dll");
        putDword(ifeo, L"fixture.exe", L"VerifierFlags", 0x80000020);
        putText(ifeo, L"fixture.exe", L"Debugger", L"\"" TARGET L"\\VistaRun.exe\" --ifeo");
        putText(ifeo, L"fixture.exe", L"KEX_VistaDebugger", L"\"" TARGET L"\\VistaRun.exe\" --ifeo");
    }
}
static BOOL storeExists(void)
{
    HKEY store; LONG error = RegOpenKeyEx(machine, L"VXsoft\\VxKexVistaPreserved", 0,
        KEY_READ | KEY_WOW64_64KEY, &store);
    if (!error) RegCloseKey(store); return !error;
}
static void activeState(void)
{
    check(dword(ifeo64, L"fixture.exe", L"KEX_WinVerSpoof", 10) &&
        text(ifeo64, L"fixture.exe", L"Debugger", L"\"" TARGET L"\\VistaRun.exe\" --ifeo"), "active native profile restored");
    check((text(ifeo64, L"fixture.exe", L"VerifierDlls", L"Foreign.dll KexDll.dll") ||
        text(ifeo64, L"fixture.exe", L"VerifierDlls", L"Foreign.dll kexdll.dll")) &&
        dword(ifeo64, L"fixture.exe", L"GlobalFlag", 0x102) && dword(ifeo64, L"fixture.exe", L"VerifierFlags", 0x80000020),
        "active native verifier and foreign options restored");
    check(dword(ifeo32, L"fixture.exe", L"KEX_WinVerSpoof", 11) &&
        dword(ifeo32, L"fixture.exe", L"GlobalFlag", 2), "disabled WOW profile remains disabled");
    check(text(machine, PRODUCT, L"KexDir", TARGET), "installation marker present");
    check(GetFileAttributes(TARGET L"\\VistaRun.exe") != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributes(TREE L"\\Native\\KexDll.dll") != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributes(TREE L"\\Wow\\KexDll.dll") != INVALID_FILE_ATTRIBUTES, "installation files present");
    check(text(context.ClassesRoot, L"VxKexVista.Log", L"VxKexOwnerPath", TARGET L"\\VxlView.exe"), "viewer association present");
    check(text(ifeo64, TEMPLATE, L"VerifierDlls", L"KexDll.dll") &&
        text(ifeo32, TEMPLATE, L"VerifierDlls", L"KexDll.dll"), "both propagation templates enabled");
}
void __cdecl mainCRTStartup(void)
{
    HKEY key; HANDLE file, tx; LONG error; DWORD written;
    log = fopen(LOGFILE, "w"); if (!log) ExitProcess(2);
    error = RegOpenKeyEx(HKEY_CURRENT_USER, ROOT, 0, KEY_READ, &key);
    if (error != ERROR_FILE_NOT_FOUND || GetFileAttributes(TREE) != INVALID_FILE_ATTRIBUTES) {
        if (!error) RegCloseKey(key); check(FALSE, "refuse existing fixture"); fclose(log); ExitProcess(3);
    }
    check(CreateDirectory(TREE, NULL), "create file fixture root");
    check(CreateDirectory(TREE L"\\Native", NULL), "create native fixture");
    check(CreateDirectory(TREE L"\\Wow", NULL), "create WOW fixture");
    root = child(HKEY_CURRENT_USER, ROOT); machine = child(root, L"Machine"); user = child(root, L"User");
    ifeo64 = child(root, L"Ifeo64"); ifeo32 = child(root, L"Ifeo32");
    context.Package = L"C:\\VxKexProbe\\NextParity\\RealPackage";
    context.Target = TARGET; context.NativeSystem = TREE L"\\Native"; context.WowSystem = TREE L"\\Wow";
    context.MachineSoftware = machine; context.UserSoftware = user;
    context.NativeIfeo = ifeo64; context.WowIfeo = ifeo32;
    context.ClassesRoot = child(root, L"Classes"); context.InstalledVersion = 101;
    profile(ifeo64, 10, TRUE); profile(ifeo32, 11, FALSE);
    check(!setup(TRUE, TRUE, FALSE), "fresh install check succeeds");
    check(GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES && text(machine, PRODUCT, L"KexDir", NULL), "fresh check leaves no deployment or settings");
    check(!setup(TRUE, TRUE, TRUE), "fresh install commit succeeds"); activeState();
    check(GetFileAttributes(TARGET L"\\Logs") != INVALID_FILE_ATTRIBUTES, "create log directory in same transaction");
    putText(machine, PRODUCT, L"LogDir", L"C:\\Fixture Custom Logs"); putDword(user, PRODUCT, L"EnableLogging", 0);
    file = CreateFile(TREE L"\\Native\\KxUnrelated.dll", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    check(file != INVALID_HANDLE_VALUE && WriteFile(file, "foreign", 7, &written, NULL), "create unrelated native file");
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    file = CreateFile(TREE L"\\Wow\\KxUser.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(file != INVALID_HANDLE_VALUE, "lock late removal destination");
    check(setup(FALSE, TRUE, TRUE) == ERROR_SHARING_VIOLATION, "late removal failure cancels whole setup");
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    activeState(); check(!storeExists(), "failed removal leaves no preservation store");
    check(!setup(FALSE, TRUE, TRUE), "commit keep-settings uninstall");
    check(storeExists(), "preserved store persists after uninstall commit");
    check(text(ifeo64, L"fixture.exe", L"KEX_WinVerSpoof", NULL) && text(ifeo64, L"fixture.exe", L"Debugger", NULL), "uninstall disables owned native profile and launcher");
    check(text(ifeo64, L"fixture.exe", L"VerifierDlls", L"Foreign.dll ") &&
        dword(ifeo64, L"fixture.exe", L"GlobalFlag", 0x102), "uninstall retains foreign verifier");
    check(GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES && text(machine, PRODUCT, L"KexDir", NULL), "uninstall removes files and installation marker");
    check(text(machine, PRODUCT, L"LogDir", L"C:\\Fixture Custom Logs") && dword(user, PRODUCT, L"EnableLogging", 0), "uninstall retains global and user preferences");
    putText(ifeo64, L"fixture.exe", L"Debugger", L"foreign-debugger");
    context.InstalledVersion = 102;
    check(setup(TRUE, TRUE, TRUE) == ERROR_ALREADY_EXISTS, "reinstall refuses conflicting debugger");
    check(storeExists() && GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES &&
        text(machine, PRODUCT, L"KexDir", NULL) && text(ifeo64, L"fixture.exe", L"Debugger", L"foreign-debugger"), "failed restore retains saved settings and cancels file deployment");
    error = RegOpenKeyEx(ifeo64, L"fixture.exe", 0, KEY_ALL_ACCESS, &key);
    check(!error, "open fixture debugger key");
    if (!error) { check(!RegDeleteValue(key, L"Debugger"), "remove deliberately conflicting debugger"); RegCloseKey(key); }
    check(!setup(TRUE, TRUE, TRUE), "commit reinstall using previously committed preservation store"); activeState();
    check(!storeExists(), "successful reinstall consumes committed preservation store");
    check(text(machine, PRODUCT, L"LogDir", L"C:\\Fixture Custom Logs") && dword(user, PRODUCT, L"EnableLogging", 0), "reinstall retains custom preferences");
    check(!setup(FALSE, FALSE, TRUE), "commit remove-all uninstall");
    check(GetFileAttributes(TARGET) == INVALID_FILE_ATTRIBUTES && !storeExists() &&
        text(machine, PRODUCT, L"LogDir", NULL) && text(user, PRODUCT, L"EnableLogging", NULL), "remove-all removes files and all product preferences");
    check(text(ifeo64, L"fixture.exe", L"VerifierDlls", L"Foreign.dll ") &&
        GetFileAttributes(TREE L"\\Native\\KxUnrelated.dll") != INVALID_FILE_ATTRIBUTES, "remove-all retains foreign verifier and system file");
    RegCloseKey(machine); RegCloseKey(user); RegCloseKey(ifeo64); RegCloseKey(ifeo32); RegCloseKey(context.ClassesRoot);
    check(!RegDeleteTree(root, NULL), "clean dedicated registry"); RegCloseKey(root);
    check(!RegDeleteKey(HKEY_CURRENT_USER, ROOT), "remove dedicated registry root");
    tx = begin(); error = KxCfgpRemoveSetupDirectory(TREE, tx); check(!error, "clean file fixture"); finish(tx, !error);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
