#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#define FIXTURE L"Software\\VxKexParityContextMenu20261001"
#define EXEKEY L"exefile\\shell\\open_vxkex"
#define MSIKEY L"Msi.Package\\shell\\open_vxkex"
LONG KxCfgpUpdateContextMenu(HKEY, PCWSTR, PCWSTR, BOOLEAN, BOOLEAN, HANDLE);
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR text) { fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", text, GetLastError()); fflush(log); if (!ok) ++failures; }
static BOOL equal(HKEY root, PCWSTR path, PCWSTR name, PCWSTR value)
{
    HKEY key; WCHAR actual[MAX_PATH * 2 + 64]; BOOL ok;
    if (RegOpenKeyEx(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key)) return FALSE;
    ok = !RegReadString(key, NULL, name, actual, ARRAYSIZE(actual)) && !wcscmp(actual, value); RegCloseKey(key); return ok;
}
static BOOL exists(HKEY root, PCWSTR path)
{
    HKEY key; LONG error = RegOpenKeyEx(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!error) RegCloseKey(key); return !error;
}
static LONG update(HKEY root, BOOL enable, BOOL extended, BOOL rollback)
{
    HANDLE tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL); LONG error;
    if (tx == INVALID_HANDLE_VALUE) return GetLastError();
    error = KxCfgpUpdateContextMenu(root, L"C:\\VxKex", L"C:\\Windows", (BOOLEAN)enable, (BOOLEAN)extended, tx);
    if (error || rollback) check(RollbackTransaction(tx), "rollback context menu transaction");
    else check(CommitTransaction(tx), "commit context menu transaction");
    CloseHandle(tx); return error;
}
static void write(HKEY root, PCWSTR path, PCWSTR name, PCWSTR value)
{
    HKEY key; LONG error = RegCreateKeyEx(root, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL);
    check(!error, "open owned fixture field"); if (error) return;
    check(!RegWriteString(key, NULL, name, value), "write fixture field"); RegCloseKey(key);
}
VOID __cdecl mainCRTStartup(VOID)
{
    HKEY root = NULL; DWORD disposition; LONG error;
#ifdef _WIN64
    log = fopen("C:\\VxKexProbe\\NextParity\\context-menu-x64.txt", "wt");
#else
    log = fopen("C:\\VxKexProbe\\NextParity\\context-menu-x86.txt", "wt");
#endif
    if (!log) ExitProcess(2);
    error = RegCreateKeyEx(HKEY_CURRENT_USER, FIXTURE, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &root, &disposition);
    check(!error && disposition == REG_CREATED_NEW_KEY, "never adopt a preexisting diagnostic root");
    if (error || disposition != REG_CREATED_NEW_KEY) goto Done;
    check(!update(root, TRUE, TRUE, TRUE) && !exists(root, EXEKEY) && !exists(root, MSIKEY), "rollback initial EXE and MSI creation together");
    check(!update(root, TRUE, TRUE, FALSE), "enable extended EXE and MSI menus");
    check(equal(root, EXEKEY L"\\command", NULL, L"\"C:\\VxKex\\VistaRun.exe\" --with-kex \"%1\""), "EXE uses shipped VistaRun instead of absent NEXT loader");
    check(equal(root, MSIKEY L"\\command", NULL, L"\"C:\\VxKex\\VistaRun.exe\" --with-kex \"C:\\Windows\\System32\\msiexec.exe\" /i \"%1\""), "MSI invokes native msiexec through VistaRun with quoted package argument");
    check(equal(root, EXEKEY, L"Extended", L"") && equal(root, MSIKEY, L"Extended", L""), "both menus use extended marker");
    check(!update(root, TRUE, FALSE, FALSE) && !equal(root, EXEKEY, L"Extended", L"") && !equal(root, MSIKEY, L"Extended", L""), "switch to normal menu without deleting registration trees");
    write(root, EXEKEY, L"ForeignValue", L"keep-me"); write(root, EXEKEY L"\\command\\ForeignChild", NULL, L"keep-child");
    check(!update(root, FALSE, FALSE, FALSE), "disable owned menu values");
    check(equal(root, EXEKEY, L"ForeignValue", L"keep-me") && equal(root, EXEKEY L"\\command\\ForeignChild", NULL, L"keep-child"), "disable preserves unrelated values and nested children");
    check(!equal(root, EXEKEY L"\\command", NULL, L"\"C:\\VxKex\\VistaRun.exe\" --with-kex \"%1\"") && !exists(root, MSIKEY), "remove only owned values and empty registration keys");
    check(!RegDeleteTree(root, EXEKEY), "reset own EXE fixture");
    write(root, MSIKEY, NULL, L"Another product"); write(root, MSIKEY L"\\command", NULL, L"foreign.exe \"%1\"");
    error = update(root, TRUE, TRUE, FALSE);
    check(error == ERROR_ALREADY_EXISTS && !exists(root, EXEKEY) && equal(root, MSIKEY L"\\command", NULL, L"foreign.exe \"%1\""), "late MSI ownership conflict rolls back prior EXE writes");
    check(!update(root, FALSE, FALSE, FALSE) && equal(root, MSIKEY, NULL, L"Another product"), "disable never removes foreign menu registration");
    check(!RegDeleteTree(root, MSIKEY), "reset own foreign MSI fixture");
    write(root, EXEKEY, NULL, L"Run with VxKex enabled"); write(root, EXEKEY L"\\command", NULL, L"\"C:\\VxKex\\VxKexLdr.exe\" \"%1\"");
    check(!update(root, TRUE, FALSE, FALSE) && equal(root, EXEKEY L"\\command", NULL, L"\"C:\\VxKex\\VistaRun.exe\" --with-kex \"%1\""), "upgrade known NEXT registration to working Vista command");
    check(!update(root, FALSE, FALSE, FALSE) && !exists(root, EXEKEY) && !exists(root, MSIKEY), "remove clean owned pair completely");
    check(!RegDeleteTree(root, NULL), "clear only diagnostic contents"); RegCloseKey(root); root = NULL;
    check(!RegDeleteKeyEx(HKEY_CURRENT_USER, FIXTURE, KEY_WOW64_64KEY, 0), "remove dedicated diagnostic root");
Done:
    if (root) RegCloseKey(root); fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
