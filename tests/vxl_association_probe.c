#include "../VxlView/vxlview.h"
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\association-result-x64.txt"
#else
#define SUFFIX L"x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\association-result-x86.txt"
#endif
static HKEY root;
static FILE *log;
static int failures;
static PCWSTR executable = L"C:\\Vxl Association Fixture\\VxlView.exe";
static void expect(LONG got, LONG wanted, PCSTR operation)
{
    fprintf(log, "%s=%ld expected=%ld\n", operation, got, wanted);
    if (got != wanted) ++failures;
}
static void expectString(PCWSTR path, PCWSTR name, PCWSTR expected)
{
    HKEY key;
    DWORD type = 0, bytes;
    WCHAR value[512];
    LONG error = RegOpenKeyEx(root, path, 0, KEY_READ, &key);
    if (!expected) {
        if (!error) { ++failures; RegCloseKey(key); }
        else if (error != ERROR_FILE_NOT_FOUND) ++failures;
        return;
    }
    if (error) { ++failures; return; }
    bytes = sizeof(value);
    error = RegQueryValueEx(key, name, NULL, &type, (PBYTE)value, &bytes);
    if (error || type != REG_SZ || bytes < 2 || bytes > sizeof(value) ||
        value[bytes / 2 - 1] || wcscmp(value, expected)) ++failures;
    RegCloseKey(key);
}
static void setString(PCWSTR path, PCWSTR name, PCWSTR value)
{
    HKEY key;
    LONG error = RegCreateKeyEx(root, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    if (error) { ++failures; return; }
    expect(RegSetValueEx(key, name, 0, REG_SZ, (PCBYTE)value,
        (DWORD)((wcslen(value) + 1) * 2)), 0, "Set fixture string");
    RegCloseKey(key);
}
VOID __cdecl AssociationProbeEntry(VOID)
{
    PCWSTR fixture = L"Software\\VxKexNextParityAssociation-20261001-" SUFFIX;
    DWORD disposition;
    HKEY key;
    LONG error;
    DWORD invalid = 42;
    log = fopen(LOGFILE, "wt");
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    error = RegCreateKeyEx(HKEY_CURRENT_USER, fixture, 0, NULL, 0,
        KEY_ALL_ACCESS, NULL, &root, &disposition);
    if (error || disposition != REG_CREATED_NEW_KEY) {
        fprintf(log, "Fixture unavailable or exists=%ld\n", error);
        if (!error) RegCloseKey(root);
        fclose(log); ExitProcess(3);
    }
    expect(VxlUpdateAssociation(root, executable, TRUE), 0, "Register");
    expectString(L".vxl", NULL, L"VxKexVista.Log");
    expectString(L"VxKexVista.Log\\shell\\open\\command", NULL,
        L"\"C:\\Vxl Association Fixture\\VxlView.exe\" \"%1\"");
    expect(VxlUpdateAssociation(root, executable, TRUE), 0, "Register again");
    expect(VxlUpdateAssociation(root, executable, FALSE), 0, "Unregister");
    expectString(L".vxl", NULL, NULL);
    expectString(L"VxKexVista.Log", NULL, NULL);
    expect(VxlUpdateAssociation(root, executable, FALSE), 0, "Unregister again");
    setString(L".vxl", NULL, L"Foreign.Log");
    setString(L".vxl\\OpenWithProgids", L"Foreign.Log", L"");
    expect(VxlUpdateAssociation(root, executable, TRUE), 0, "Register foreign default");
    expectString(L".vxl", NULL, L"Foreign.Log");
    expectString(L".vxl\\OpenWithProgids", L"VxKexVista.Log", L"");
    setString(L"VxKexVista.Log\\shell\\open\\command", NULL, L"foreign-command");
    expect(VxlUpdateAssociation(root, executable, FALSE), ERROR_ALREADY_EXISTS, "Reject changed command");
    expectString(L"VxKexVista.Log", L"VxKexOwnerPath", executable);
    setString(L"VxKexVista.Log\\shell\\open\\command", NULL,
        L"\"C:\\Vxl Association Fixture\\VxlView.exe\" \"%1\"");
    expect(VxlUpdateAssociation(root, executable, FALSE), 0, "Unregister foreign default");
    expectString(L".vxl", NULL, L"Foreign.Log");
    expectString(L".vxl\\OpenWithProgids", L"Foreign.Log", L"");
    expectString(L"VxKexVista.Log", NULL, NULL);
    error = RegOpenKeyEx(root, L".vxl", 0, KEY_WRITE, &key);
    if (!error) {
        expect(RegSetValueEx(key, NULL, 0, REG_DWORD, (PCBYTE)&invalid, sizeof(invalid)), 0, "Set invalid default");
        RegCloseKey(key);
    } else ++failures;
    expect(VxlUpdateAssociation(root, executable, TRUE), ERROR_INVALID_DATA, "Rollback invalid default");
    expectString(L"VxKexVista.Log", NULL, NULL);
    expect(RegDeleteTree(root, NULL), 0, "Clean fixture contents");
    RegCloseKey(root);
    expect(RegDeleteKey(HKEY_CURRENT_USER, fixture), 0, "Clean fixture root");
    fprintf(log, "Failures=%d\n", failures);
    fclose(log); ExitProcess(failures ? 1 : 0);
}
