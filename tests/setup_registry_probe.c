#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define ROOT L"Software\\VxKexSetupRegistry-x64"
#define FILEPATH L"C:\\VxKexProbe\\NextParity\\SetupRegistryFile-x64.txt"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-registry-x64.txt"
#else
#define ROOT L"Software\\VxKexSetupRegistry-x86"
#define FILEPATH L"C:\\VxKexProbe\\NextParity\\SetupRegistryFile-x86.txt"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-registry-x86.txt"
#endif
#define TARGET L"C:\\VxKexFixture"
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
static LONG install(HANDLE tx, DWORD version)
{
    LONG error = KxCfgpConfigureSetupSettings(machine, user, TARGET, version, TRUE, TRUE, tx);
    if (!error) error = KxCfgpConfigurePropagationTemplate(ifeo64, KEY_WOW64_64KEY, TRUE, tx);
    if (!error) error = KxCfgpConfigurePropagationTemplate(ifeo32, KEY_WOW64_32KEY, TRUE, tx);
    return error;
}
static LONG uninstall(HANDLE tx, BOOL keep)
{
    LONG error = KxCfgpConfigureSetupSettings(machine, user, TARGET, 0, FALSE, keep, tx);
    if (!error) error = KxCfgpConfigurePropagationTemplate(ifeo64, KEY_WOW64_64KEY, FALSE, tx);
    if (!error) error = KxCfgpConfigurePropagationTemplate(ifeo32, KEY_WOW64_32KEY, FALSE, tx);
    return error;
}
void __cdecl mainCRTStartup(void)
{
    HKEY key; HANDLE tx, file; LONG error; DWORD written; CHAR bytes[16];
    log = fopen(LOGFILE, "w"); if (!log) ExitProcess(2);
    error = RegOpenKeyEx(HKEY_CURRENT_USER, ROOT, 0, KEY_READ, &key);
    if (error != ERROR_FILE_NOT_FOUND || GetFileAttributes(FILEPATH) != INVALID_FILE_ATTRIBUTES) {
        if (!error) RegCloseKey(key); check(FALSE, "refuse existing fixture"); fclose(log); ExitProcess(3);
    }
    root = child(HKEY_CURRENT_USER, ROOT); machine = child(root, L"Machine"); user = child(root, L"User");
    ifeo64 = child(root, L"Ifeo64"); ifeo32 = child(root, L"Ifeo32");
    tx = begin(); error = install(tx, 101); check(!error, "stage fresh setup");
    check(text(machine, PRODUCT, L"KexDir", NULL), "fresh setup invisible outside transaction"); finish(tx, FALSE);
    check(text(machine, PRODUCT, L"KexDir", NULL) && text(ifeo64, TEMPLATE, L"VerifierDlls", NULL), "fresh setup rollback");
    tx = begin(); error = install(tx, 101); check(!error, "stage committed setup"); finish(tx, !error);
    check(text(machine, PRODUCT, L"KexDir", TARGET) && dword(machine, PRODUCT, L"InstalledVersion", 101), "commit installation markers");
    check(text(machine, PRODUCT, L"LogDir", TARGET L"\\Logs"), "set default log directory");
    check(text(ifeo64, TEMPLATE, L"VerifierDlls", L"KexDll.dll") &&
        text(ifeo32, TEMPLATE, L"VerifierDlls", L"KexDll.dll"), "commit both templates");
    check(dword(ifeo64, TEMPLATE, L"VerifierFlags", 0x80000000) && dword(ifeo32, TEMPLATE, L"GlobalFlag", 0x100), "safe native verifier defaults");
    putText(machine, PRODUCT, L"LogDir", L"C:\\Custom Logs");
    putDword(user, PRODUCT, L"EnableLogging", 0);
    putText(machine, L"VXsoft\\OtherProduct", NULL, L"foreign-machine");
    putText(user, L"VXsoft\\OtherProduct", NULL, L"foreign-user");
    putText(ifeo64, TEMPLATE, L"VerifierDlls", L"Foreign.dll KexDll.dll KEXDLL.DLL");
    putDword(ifeo64, TEMPLATE, L"GlobalFlag", 0x102); putDword(ifeo64, TEMPLATE, L"VerifierFlags", 0x20);
    putText(ifeo64, TEMPLATE L"\\ForeignChild", NULL, L"foreign-child");
    tx = begin(); error = install(tx, 102); check(!error, "stage update preserving preferences"); finish(tx, !error);
    check(text(machine, PRODUCT, L"LogDir", L"C:\\Custom Logs") && dword(user, PRODUCT, L"EnableLogging", 0), "retain global and user preferences");
    check(text(ifeo64, TEMPLATE, L"VerifierDlls", L"Foreign.dll  KexDll.dll") && dword(ifeo64, TEMPLATE, L"GlobalFlag", 0x102) &&
        dword(ifeo64, TEMPLATE, L"VerifierFlags", 0x20), "retain foreign verifier fields and remove duplicate Kex tokens");
    tx = begin(); error = install(tx, 102); check(!error, "stage repeated update"); finish(tx, !error);
    check(text(ifeo64, TEMPLATE, L"VerifierDlls", L"Foreign.dll  KexDll.dll"), "repeated update preserves provider separators");
    // A malformed setting must cancel a previously staged real-file update.
    file = CreateFile(FILEPATH, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    check(file != INVALID_HANDLE_VALUE, "create file rollback fixture");
    check(WriteFile(file, "old-file", 8, &written, NULL) && written == 8, "write old file bytes"); CloseHandle(file);
    putDword(machine, PRODUCT, L"LogDir", 7);
    tx = begin(); check(!KxCfgpCopySetupFile(L"C:\\VxKexProbe\\NextParity\\RealPackage\\KexDll.dll", FILEPATH, tx), "stage real DLL before settings failure");
    check(install(tx, 103) == ERROR_INVALID_DATA, "reject malformed log directory"); finish(tx, FALSE);
    file = CreateFile(FILEPATH, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(file != INVALID_HANDLE_VALUE && ReadFile(file, bytes, sizeof(bytes), &written, NULL) && written == 8 && !memcmp(bytes, "old-file", 8), "registry failure restores file bytes");
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    check(dword(machine, PRODUCT, L"InstalledVersion", 102), "registry failure preserves installed version");
    putText(machine, PRODUCT, L"LogDir", L"C:\\Custom Logs");
    putText(ifeo32, TEMPLATE, L"GlobalFlag", L"invalid-dword");
    tx = begin(); check(install(tx, 103) == ERROR_INVALID_DATA, "reject malformed propagation flags after settings staged"); finish(tx, FALSE);
    check(dword(machine, PRODUCT, L"InstalledVersion", 102) && text(ifeo32, TEMPLATE, L"GlobalFlag", L"invalid-dword"),
        "template failure rolls back global settings and retains original invalid value");
    putDword(ifeo32, TEMPLATE, L"GlobalFlag", 0x100);
    putText(ifeo32, TEMPLATE, L"VerifierDlls", L" \t"); putDword(ifeo32, TEMPLATE, L"VerifierFlags", 0);
    tx = begin(); error = install(tx, 102); check(!error, "stage whitespace-only provider list"); finish(tx, !error);
    check(text(ifeo32, TEMPLATE, L"VerifierDlls", L"KexDll.dll") && dword(ifeo32, TEMPLATE, L"VerifierFlags", 0x80000000),
        "whitespace-only list uses safe Kex-only verifier defaults");
    tx = begin(); check(KxCfgpConfigureSetupSettings(machine, user, L"C:\\Different", 103, TRUE, TRUE, tx) == ERROR_ALREADY_EXISTS, "reject different installation directory"); finish(tx, FALSE);
    tx = begin(); error = uninstall(tx, TRUE); check(!error, "stage keep-settings uninstall"); finish(tx, FALSE);
    check(text(machine, PRODUCT, L"KexDir", TARGET), "uninstall rollback restores installation marker");
    tx = begin(); error = uninstall(tx, TRUE); check(!error, "stage committed keep-settings uninstall"); finish(tx, !error);
    check(text(machine, PRODUCT, L"KexDir", NULL) && text(machine, PRODUCT, L"InstalledVersion", NULL), "keep mode removes installation markers");
    check(text(machine, PRODUCT, L"LogDir", L"C:\\Custom Logs") && dword(user, PRODUCT, L"EnableLogging", 0), "keep mode retains preferences");
    check(text(ifeo64, TEMPLATE, L"VerifierDlls", L"Foreign.dll  ") && dword(ifeo64, TEMPLATE, L"GlobalFlag", 0x102) &&
        dword(ifeo64, TEMPLATE, L"VerifierFlags", 0x20), "keep mode retains foreign verifier settings");
    check(text(ifeo32, TEMPLATE, L"VerifierDlls", NULL) && dword(ifeo32, TEMPLATE, L"GlobalFlag", 0), "disable plain WOW template");
    tx = begin(); error = install(tx, 104); check(!error, "stage reinstall"); finish(tx, !error);
    check(dword(machine, PRODUCT, L"InstalledVersion", 104) && text(machine, PRODUCT, L"LogDir", L"C:\\Custom Logs"), "reinstall restores markers and keeps log directory");
    putText(machine, L"VXsoft\\VxKexLdr", L"Legacy", L"old"); putText(user, L"VXsoft\\VxKexLdr", L"Legacy", L"old");
    tx = begin(); error = uninstall(tx, FALSE); check(!error, "stage remove-all settings uninstall"); finish(tx, !error);
    check(text(machine, PRODUCT, L"LogDir", NULL) && text(user, PRODUCT, L"EnableLogging", NULL) &&
        text(machine, L"VXsoft\\VxKexLdr", L"Legacy", NULL) && text(user, L"VXsoft\\VxKexLdr", L"Legacy", NULL), "remove product settings from both roots");
    check(text(machine, L"VXsoft\\OtherProduct", NULL, L"foreign-machine") && text(user, L"VXsoft\\OtherProduct", NULL, L"foreign-user") &&
        text(ifeo64, TEMPLATE L"\\ForeignChild", NULL, L"foreign-child"), "remove-all retains foreign products and template children");
    RegCloseKey(machine); RegCloseKey(user); RegCloseKey(ifeo64); RegCloseKey(ifeo32);
    check(!RegDeleteTree(root, NULL), "clean dedicated registry"); RegCloseKey(root);
    check(!RegDeleteKey(HKEY_CURRENT_USER, ROOT), "remove fixture root"); check(DeleteFile(FILEPATH), "remove file fixture");
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
