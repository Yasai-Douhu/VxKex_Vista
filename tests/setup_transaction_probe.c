#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#include <winioctl.h>
#ifdef _WIN64
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupTransaction-x64"
#define REGROOT L"Software\\VxKexSetupTransaction-x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-transaction-x64.txt"
#else
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupTransaction-x86"
#define REGROOT L"Software\\VxKexSetupTransaction-x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-transaction-x86.txt"
#endif
#define SHELL_CLASS L"CLSID\\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}"
#define SHELL_DLL ROOT L"\\Target\\KexShlEx.dll"
static FILE *log;
static unsigned failures;
static HKEY key;
static void check(BOOL ok, PCSTR name)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", name, GetLastError());
    fflush(log); if (!ok) ++failures;
}
static BOOL writeFile(PCWSTR path, PCSTR value)
{
    DWORD written, length = (DWORD)strlen(value);
    HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = WriteFile(file, value, length, &written, NULL) && written == length;
    CloseHandle(file); return ok;
}
static BOOL readFile(PCWSTR path, PCSTR value, HANDLE transaction)
{
    CHAR data[64]; DWORD read; BOOL ok;
    HANDLE file = transaction ? CreateFileTransacted(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0,
        NULL, transaction, NULL, NULL) : CreateFile(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, data, sizeof(data), &read, NULL) && read == strlen(value) && !memcmp(data, value, read);
    CloseHandle(file); return ok;
}
static DWORD regValue(void)
{
    DWORD value = 0, size = sizeof(value);
    RegQueryValueEx(key, L"State", NULL, NULL, (PBYTE)&value, &size); return value;
}
static BOOL registryString(PCWSTR path, PCWSTR name, PCWSTR expected)
{
    WCHAR text[MAX_PATH + 32]; DWORD bytes = sizeof(text), type; HKEY entry;
    LONG error = RegOpenKeyEx(key, path, 0, KEY_READ, &entry);
    if (error) return !expected && error == ERROR_FILE_NOT_FOUND;
    error = RegQueryValueEx(entry, name, NULL, &type, (PBYTE)text, &bytes);
    RegCloseKey(entry);
    if (!expected) return error == ERROR_FILE_NOT_FOUND;
    return !error && type == REG_SZ && bytes == (wcslen(expected) + 1) * sizeof(WCHAR) &&
        !memcmp(text, expected, bytes);
}
static BOOL setRegistryString(PCWSTR path, PCWSTR name, PCWSTR value)
{
    HKEY entry; LONG error = RegCreateKeyEx(key, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &entry, NULL);
    if (error) return FALSE;
    error = RegSetValueEx(entry, name, 0, REG_SZ, (PCBYTE)value, (DWORD)((wcslen(value) + 1) * 2));
    RegCloseKey(entry); return !error;
}
static BOOL hasAssociation(BOOL expected)
{
    HKEY command; LONG error; WCHAR text[MAX_PATH + 32]; DWORD bytes = sizeof(text), type;
    error = RegOpenKeyEx(key, L"VxKexVista.Log\\shell\\open\\command", 0, KEY_READ, &command);
    if (!expected) {
        if (!error) RegCloseKey(command);
        return error == ERROR_FILE_NOT_FOUND;
    }
    if (error) return FALSE;
    error = RegQueryValueEx(command, NULL, NULL, &type, (PBYTE)text, &bytes);
    RegCloseKey(command);
    return !error && type == REG_SZ && bytes == sizeof(L"\"" ROOT L"\\Target\\VxlView.exe\" \"%1\"") &&
        !memcmp(text, L"\"" ROOT L"\\Target\\VxlView.exe\" \"%1\"", bytes);
}
static BOOL stageRegistry(HANDLE transaction, DWORD value)
{
    HKEY transacted; LONG error;
    error = RegOpenKeyTransacted(HKEY_CURRENT_USER, REGROOT, 0, KEY_ALL_ACCESS,
        &transacted, transaction, NULL);
    if (error) return FALSE;
    error = RegSetValueEx(transacted, L"State", 0, REG_DWORD, (PCBYTE)&value, sizeof(value));
    RegCloseKey(transacted); return !error;
}
static BOOL junction(PCWSTR path, PCWSTR target)
{
    struct {
        DWORD Tag; WORD Length, Reserved;
        WORD SubstituteOffset, SubstituteLength, PrintOffset, PrintLength;
        WCHAR Names[2 * MAX_PATH];
    } data;
    WCHAR native[MAX_PATH]; DWORD returned; HANDLE directory; BOOL ok;
    ZeroMemory(&data, sizeof(data));
    if (FAILED(StringCchPrintf(native, ARRAYSIZE(native), L"\\??\\%s", target))) return FALSE;
    data.Tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.SubstituteLength = (WORD)(wcslen(native) * sizeof(WCHAR));
    data.PrintOffset = data.SubstituteLength + sizeof(WCHAR);
    data.PrintLength = (WORD)(wcslen(target) * sizeof(WCHAR));
    data.Length = 8 + data.PrintOffset + data.PrintLength + sizeof(WCHAR);
    memcpy(data.Names, native, data.SubstituteLength + sizeof(WCHAR));
    memcpy((PBYTE)data.Names + data.PrintOffset, target, data.PrintLength + sizeof(WCHAR));
    if (!CreateDirectory(path, NULL)) return FALSE;
    directory = CreateFile(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (directory == INVALID_HANDLE_VALUE) return FALSE;
    ok = DeviceIoControl(directory, FSCTL_SET_REPARSE_POINT, &data, data.Length + 8,
        NULL, 0, &returned, NULL); CloseHandle(directory); return ok;
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
void __cdecl mainCRTStartup(void)
{
    HANDLE tx, locked; DWORD value = 1, disposition; LONG error; HKEY existing; BOOL staged;
    log = fopen(LOGFILE, "w"); if (!log) ExitProcess(2);
    if (GetFileAttributes(ROOT) != INVALID_FILE_ATTRIBUTES ||
        RegOpenKeyEx(HKEY_CURRENT_USER, REGROOT, 0, KEY_READ, &existing) != ERROR_FILE_NOT_FOUND) {
        check(FALSE, "refuse existing fixture"); fclose(log); ExitProcess(3);
    }
    check(CreateDirectory(ROOT, NULL), "create fixture");
    check(CreateDirectory(ROOT L"\\Source", NULL), "create source");
    check(CreateDirectory(ROOT L"\\Source\\Nested", NULL), "create nested source");
    check(CreateDirectory(ROOT L"\\Target", NULL), "create target");
    check(writeFile(ROOT L"\\Source\\a.txt", "new-a"), "write source a");
    check(writeFile(ROOT L"\\Source\\b.txt", "new-b"), "write source b");
    check(writeFile(ROOT L"\\Source\\Nested\\\u65e5\u672c\u8a9e.txt", "unicode"), "write Unicode source");
    check(writeFile(ROOT L"\\Target\\a.txt", "old-a"), "write old target");
    check(writeFile(ROOT L"\\Target\\b.txt", "old-b"), "write old target b");
    check(writeFile(ROOT L"\\Target\\unrelated.txt", "keep"), "write unrelated target");
    check(!RegCreateKeyEx(HKEY_CURRENT_USER, REGROOT, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, &disposition), "create registry fixture");
    check(!RegSetValueEx(key, L"State", 0, REG_DWORD, (PCBYTE)&value, 4), "write registry fixture");
    if (failures) { fclose(log); ExitProcess(4); }

    tx = begin();
    error = KxCfgpCopySetupDirectory(ROOT L"\\Source", ROOT L"\\Target", tx);
    fprintf(log, "CopyTreeStatus=%ld\n", error); check(!error, "stage directory merge");
    check(stageRegistry(tx, 2), "stage registry with filesystem");
    check(!KxCfgpUpdateLogAssociation(key, ROOT L"\\Target\\VxlView.exe", TRUE, tx), "stage association with filesystem");
    check(!KxCfgpUpdateShellExtension(key, key, SHELL_DLL, TRUE, tx), "stage shell registration with filesystem");
    check(readFile(ROOT L"\\Target\\a.txt", "new-a", tx), "new bytes visible inside transaction");
    check(readFile(ROOT L"\\Target\\a.txt", "old-a", NULL), "old bytes visible outside transaction");
    check(regValue() == 1, "old registry visible outside transaction");
    check(hasAssociation(FALSE), "association invisible before commit");
    check(registryString(SHELL_CLASS L"\\InProcServer32", NULL, NULL), "shell registration invisible before commit");
    finish(tx, FALSE);
    check(readFile(ROOT L"\\Target\\a.txt", "old-a", NULL) && regValue() == 1, "rollback restores files and registry");
    check(hasAssociation(FALSE), "rollback removes staged association");
    check(registryString(SHELL_CLASS L"\\InProcServer32", NULL, NULL), "rollback removes staged shell registration");
    check(GetFileAttributes(ROOT L"\\Target\\Nested") == INVALID_FILE_ATTRIBUTES, "rollback removes staged directory");

    tx = begin();
    error = KxCfgpCopySetupDirectory(ROOT L"\\Source", ROOT L"\\Target", tx);
    staged = !error && stageRegistry(tx, 2) &&
        !KxCfgpUpdateLogAssociation(key, ROOT L"\\Target\\VxlView.exe", TRUE, tx) &&
        !KxCfgpUpdateShellExtension(key, key, SHELL_DLL, TRUE, tx);
    check(staged, "stage successful update"); finish(tx, staged);
    check(readFile(ROOT L"\\Target\\a.txt", "new-a", NULL) && regValue() == 2, "commit files and registry");
    check(hasAssociation(TRUE), "commit shared association");
    check(registryString(SHELL_CLASS L"\\InProcServer32", NULL, SHELL_DLL), "commit shared shell registration");
    check(registryString(L"exefile\\shellex\\PropertySheetHandlers\\VxKex", NULL,
        L"{9AACA888-A5F5-4C01-852E-8A2005C1D45F}"), "commit EXE handler");
    check(registryString(L"lnkfile\\shellex\\PropertySheetHandlers\\VxKex", NULL,
        L"{9AACA888-A5F5-4C01-852E-8A2005C1D45F}"), "commit shortcut handler");
    check(readFile(ROOT L"\\Target\\Nested\\\u65e5\u672c\u8a9e.txt", "unicode", NULL), "commit nested Unicode file");
    check(readFile(ROOT L"\\Target\\unrelated.txt", "keep", NULL), "merge preserves unrelated file");
    check(readFile(ROOT L"\\Source\\a.txt", "new-a", NULL), "source unchanged");

    check(writeFile(ROOT L"\\Source\\a.txt", "update-a"), "prepare second update");
    locked = CreateFile(ROOT L"\\Target\\b.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(locked != INVALID_HANDLE_VALUE, "lock second file");
    tx = begin();
    check(!KxCfgpCopySetupFile(ROOT L"\\Source\\a.txt", ROOT L"\\Target\\a.txt", tx), "stage first update before failure");
    check(stageRegistry(tx, 3), "stage registry before failure");
    check(!KxCfgpUpdateLogAssociation(key, ROOT L"\\Target\\VxlView.exe", FALSE, tx), "stage association removal before failure");
    check(!KxCfgpUpdateShellExtension(key, key, SHELL_DLL, FALSE, tx), "stage shell removal before failure");
    error = KxCfgpCopySetupFile(ROOT L"\\Source\\b.txt", ROOT L"\\Target\\b.txt", tx);
    fprintf(log, "LockedCopyStatus=%ld\n", error); check(error != 0, "locked second file rejects update");
    finish(tx, FALSE);
    check(readFile(ROOT L"\\Target\\a.txt", "new-a", NULL) && regValue() == 2, "failed update rollback restores first file and registry");
    check(hasAssociation(TRUE), "failed update restores association");
    check(registryString(SHELL_CLASS L"\\InProcServer32", NULL, SHELL_DLL), "failed update restores shell registration");
    tx = begin();
    check(!KxCfgpDeleteSetupFile(ROOT L"\\Target\\a.txt", tx), "stage deletion before failure");
    error = KxCfgpRemoveSetupDirectory(ROOT L"\\Target", tx);
    fprintf(log, "LockedRemoveStatus=%ld\n", error); check(error != 0, "locked directory removal fails");
    finish(tx, FALSE); CloseHandle(locked);
    check(readFile(ROOT L"\\Target\\a.txt", "new-a", NULL), "failed removal restores staged deletion");
    check(readFile(ROOT L"\\Target\\b.txt", "new-b", NULL), "failed removal retains locked file");

    check(CreateHardLink(ROOT L"\\OtherLink.txt", ROOT L"\\Target\\a.txt", NULL), "create hardlink fixture");
    tx = begin();
    check(KxCfgpCopySetupFile(ROOT L"\\Source\\a.txt", ROOT L"\\Target\\a.txt", tx) == ERROR_INVALID_DATA, "refuse linked destination overwrite");
    check(KxCfgpRemoveSetupDirectory(L"C:\\", tx) == ERROR_INVALID_NAME, "refuse volume root");
    check(KxCfgpDeleteSetupFile(L"C", tx) == ERROR_INVALID_NAME, "refuse short path");
    check(KxCfgpDeleteSetupFile(L"relative.txt", tx) == ERROR_INVALID_NAME, "refuse relative path");
    check(KxCfgpCopySetupDirectory(ROOT L"\\Source", ROOT L"\\Source\\Child", tx) == ERROR_INVALID_PARAMETER, "refuse overlapping trees");
    finish(tx, FALSE);
    check(readFile(ROOT L"\\OtherLink.txt", "new-a", NULL), "hardlink bytes unchanged");
    check(junction(ROOT L"\\Source\\Junction", ROOT L"\\Target"), "create junction fixture");
    tx = begin();
    check(KxCfgpDeleteSetupFile(ROOT L"\\Source\\Junction\\a.txt", tx) == ERROR_INVALID_DATA,
        "refuse reparse ancestor deletion");
    check(KxCfgpCopySetupDirectory(ROOT L"\\Source", ROOT L"\\Copy", tx) == ERROR_INVALID_DATA,
        "refuse reparse entry during directory copy");
    check(KxCfgpRemoveSetupDirectory(ROOT L"\\Source", tx) == ERROR_INVALID_DATA,
        "refuse reparse entry during directory removal");
    finish(tx, FALSE);
    check(readFile(ROOT L"\\Target\\a.txt", "new-a", NULL) &&
        readFile(ROOT L"\\Source\\a.txt", "update-a", NULL), "junction refusal preserves both trees");
    check(GetFileAttributes(ROOT L"\\Copy") == INVALID_FILE_ATTRIBUTES, "rollback removes partial copy");
    check(RemoveDirectory(ROOT L"\\Source\\Junction"), "remove junction itself");
    check(setRegistryString(SHELL_CLASS L"\\InProcServer32", L"ThreadingModel", L"ForeignModel"), "create conflicting shell field");
    tx = begin();
    check(!KxCfgpCopySetupFile(ROOT L"\\Source\\a.txt", ROOT L"\\Target\\b.txt", tx), "stage copy before shell conflict");
    check(KxCfgpUpdateShellExtension(key, key, SHELL_DLL, TRUE, tx) == ERROR_ALREADY_EXISTS, "reject conflicting shell field");
    finish(tx, FALSE);
    check(readFile(ROOT L"\\Target\\b.txt", "new-b", NULL), "shell failure rolls back file update");
    check(registryString(SHELL_CLASS L"\\InProcServer32", L"ThreadingModel", L"ForeignModel"), "retain conflicting shell field");
    check(setRegistryString(SHELL_CLASS L"\\InProcServer32", L"ThreadingModel", L"Apartment"), "restore shell fixture");
    check(setRegistryString(SHELL_CLASS, L"Foreign", L"keep"), "create unrelated class value");
    check(setRegistryString(SHELL_CLASS L"\\ForeignChild", NULL, L"keep-child"), "create unrelated class subkey");
    tx = begin();
    check(KxCfgpUpdateShellExtension(key, key, ROOT L"\\Wrong.dll", FALSE, tx) == ERROR_ALREADY_EXISTS,
        "refuse removal with different server path");
    finish(tx, FALSE);
    tx = begin();
    error = KxCfgpRemoveSetupDirectory(ROOT, tx); check(!error, "stage fixture removal");
    staged = !error && !KxCfgpUpdateLogAssociation(key, ROOT L"\\Target\\VxlView.exe", FALSE, tx) &&
        !KxCfgpUpdateShellExtension(key, key, SHELL_DLL, FALSE, tx);
    check(staged, "stage association and file removal together"); finish(tx, staged);
    check(GetFileAttributes(ROOT) == INVALID_FILE_ATTRIBUTES, "committed tree removal");
    check(hasAssociation(FALSE), "committed association removal");
    check(registryString(SHELL_CLASS L"\\InProcServer32", NULL, NULL), "committed shell removal");
    check(registryString(SHELL_CLASS, L"Foreign", L"keep") &&
        registryString(SHELL_CLASS L"\\ForeignChild", NULL, L"keep-child"), "shell removal preserves unrelated values and children");
    check(!RegDeleteTree(key, NULL), "clean dedicated registry fixture contents");
    RegCloseKey(key); check(!RegDeleteKey(HKEY_CURRENT_USER, REGROOT), "clean registry fixture");
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
