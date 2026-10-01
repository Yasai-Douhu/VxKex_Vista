#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-view-x64.txt"
#else
#define SUFFIX L"x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-view-x86.txt"
#endif
static FILE *log;
static int failures;
static HKEY fixture, source[2], backup[2];
static PCWSTR sources[] = {L"Source64", L"Source32"};
static PCWSTR backups[] = {L"Backup64", L"Backup32"};
static void check(BOOL ok, PCSTR name)
{
    fprintf(log, "%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
static void dword(HKEY key, PCWSTR name, DWORD value)
{
    check(!RegSetValueEx(key, name, 0, REG_DWORD, (PCBYTE)&value, 4), "write DWORD");
}
static void text(HKEY key, PCWSTR name, PCWSTR value)
{
    check(!RegSetValueEx(key, name, 0, REG_SZ, (PCBYTE)value,
        (DWORD)((wcslen(value) + 1) * 2)), "write text");
}
static HKEY create(HKEY key, PCWSTR path)
{
    HKEY result = NULL;
    check(!RegCreateKeyEx(key, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &result, NULL), "create fixture key");
    return result;
}
static BOOL hasDword(HKEY root, PCWSTR path, PCWSTR name, DWORD expected)
{
    HKEY key;
    DWORD type, bytes = 4, value;
    LONG error;
    if (RegOpenKeyEx(root, path, 0, KEY_READ, &key)) return FALSE;
    error = RegQueryValueEx(key, name, NULL, &type, (PBYTE)&value, &bytes);
    RegCloseKey(key);
    return !error && type == REG_DWORD && bytes == 4 && value == expected;
}
static BOOL missing(HKEY root, PCWSTR path, PCWSTR name)
{
    HKEY key;
    DWORD bytes;
    LONG error = RegOpenKeyEx(root, path, 0, KEY_READ, &key);
    if (error == ERROR_FILE_NOT_FOUND) return TRUE;
    if (error) return FALSE;
    error = RegQueryValueEx(key, name, NULL, NULL, NULL, &bytes);
    RegCloseKey(key);
    return error == ERROR_FILE_NOT_FOUND;
}
static BOOL hasText(HKEY root, PCWSTR path, PCWSTR name, PCWSTR expected)
{
    HKEY key;
    WCHAR value[512];
    DWORD type, bytes = sizeof(value);
    LONG error;
    if (RegOpenKeyEx(root, path, 0, KEY_READ, &key)) return FALSE;
    error = RegQueryValueEx(key, name, NULL, &type, (PBYTE)value, &bytes);
    RegCloseKey(key);
    return !error && type == REG_SZ && bytes >= 2 && bytes <= sizeof(value) &&
        !(bytes & 1) && !value[bytes / 2 - 1] && !wcscmp(value, expected);
}
static LONG processViews(BOOL restore, BOOL commit)
{
    HANDLE transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    HKEY a = NULL, b = NULL;
    LONG error = 0;
    unsigned i;
    if (transaction == INVALID_HANDLE_VALUE) return GetLastError();
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyTransacted(fixture, sources[i], 0, KEY_ALL_ACCESS, &a, transaction, NULL);
        if (error) break;
        error = RegOpenKeyTransacted(fixture, backups[i], 0, KEY_ALL_ACCESS, &b, transaction, NULL);
        if (!error) error = restore ? KxCfgpRestoreIfeoView(a, b, transaction, 0) :
            KxCfgpPreserveIfeoView(a, b, transaction, 0);
        if (b) { RegCloseKey(b); b = NULL; }
        RegCloseKey(a); a = NULL;
        fprintf(log, "%s view=%u status=%ld\n", restore ? "restore" : "preserve", i, error);
        if (error) break;
    }
    if (!error && commit) {
        if (!CommitTransaction(transaction)) error = GetLastError();
    } else check(RollbackTransaction(transaction), "rollback all views");
    if (a) RegCloseKey(a);
    if (b) RegCloseKey(b);
    CloseHandle(transaction);
    return error;
}
static void configure(HKEY root, PCWSTR path, DWORD value)
{
    HKEY key = create(root, path);
    if (!key) return;
    dword(key, L"KEX_WinVerSpoof", value);
    text(key, L"VerifierDlls", L"kexdll.dll");
    dword(key, L"GlobalFlag", 0x102);
    dword(key, L"VerifierFlags", 0x80000000);
    RegCloseKey(key);
}
static void nativeViews(VOID)
{
    PCWSTR path = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\VxKexPreserveNative-20261001-" SUFFIX;
    REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    PCWSTR names[] = {L"NativeBackup64", L"NativeBackup32"};
    HKEY roots[2] = {NULL, NULL}, stores[2] = {NULL, NULL}, child = NULL;
    HANDLE transaction;
    DWORD disposition, bytes, type, value;
    LONG error;
    unsigned i;
    if (RtlOperatingSystemBitness() != 64) { check(FALSE, "native test requires Vista x64"); return; }
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create native view transaction"); return; }
    for (i = 0; i < 2; ++i) {
        error = RegCreateKeyTransacted(HKEY_LOCAL_MACHINE, path, 0, NULL, 0,
            KEY_ALL_ACCESS | views[i], NULL, &roots[i], &disposition, transaction, NULL);
        check(!error && disposition == REG_CREATED_NEW_KEY, "native fixture root is new in selected view");
        if (error || disposition != REG_CREATED_NEW_KEY) goto Done;
        error = RegCreateKeyTransacted(roots[i], L"same.exe", 0, NULL, 0,
            KEY_ALL_ACCESS | views[i], NULL, &child, NULL, transaction, NULL);
        if (error) { check(FALSE, "create native profile"); goto Done; }
        dword(child, L"KEX_WinVerSpoof", 500 + i);
        text(child, L"VerifierDlls", L"kexdll.dll");
        dword(child, L"GlobalFlag", 0x100);
        dword(child, L"VerifierFlags", 0x80000000);
        RegCloseKey(child); child = NULL;
        error = RegCreateKeyTransacted(fixture, names[i], 0, NULL, 0,
            KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &stores[i], NULL, transaction, NULL);
        if (error) { check(FALSE, "create native view store"); goto Done; }
    }
    for (i = 0; i < 2; ++i) {
        error = KxCfgpPreserveIfeoView(roots[i], stores[i], transaction, views[i]);
        fprintf(log, "native preserve view=%u status=%ld\n", i, error);
        check(!error, "preserve explicit native view");
        if (error) goto Done;
    }
    for (i = 0; i < 2; ++i) {
        check(KxCfgpRestoreIfeoView(roots[i], stores[i], transaction, views[1-i]) == ERROR_INVALID_DATA,
            "store cannot be restored into opposite view");
        error = KxCfgpRestoreIfeoView(roots[i], stores[i], transaction, views[i]);
        fprintf(log, "native restore view=%u status=%ld\n", i, error);
        check(!error, "restore explicit native view");
        if (error) goto Done;
        error = RegOpenKeyTransacted(roots[i], L"same.exe", 0, KEY_READ | views[i], &child, transaction, NULL);
        if (error) { check(FALSE, "open restored native profile"); goto Done; }
        bytes = 4;
        error = RegQueryValueEx(child, L"KEX_WinVerSpoof", NULL, &type, (PBYTE)&value, &bytes);
        check(!error && type == REG_DWORD && bytes == 4 && value == 500 + i, "native WOW64 settings stay separate");
        RegCloseKey(child); child = NULL;
    }
Done:
    if (child) RegCloseKey(child);
    for (i = 0; i < 2; ++i) {
        if (roots[i]) RegCloseKey(roots[i]);
        if (stores[i]) RegCloseKey(stores[i]);
    }
    check(RollbackTransaction(transaction), "rollback native IFEO fixture");
    CloseHandle(transaction);
    for (i = 0; i < 2; ++i) {
        child = NULL;
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_READ | views[i], &child);
        check(error == ERROR_FILE_NOT_FOUND, "no native fixture remains after rollback");
        if (!error) RegCloseKey(child);
    }
}
static void deleteTransactions(VOID)
{
    HANDLE transaction;
    HKEY key = NULL;
    LONG error;
    // First exercise the actual reinstall pattern: create/commit, delete/commit.
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create deletion transaction"); return; }
    error = RegCreateKeyTransacted(fixture, L"DeleteFixture", 0, NULL, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL, transaction, NULL);
    check(!error, "create deletion fixture");
    if (!error) { RegCloseKey(key); key = NULL; }
    check(CommitTransaction(transaction), "commit deletion fixture creation");
    CloseHandle(transaction);
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create deletion transaction"); return; }
    error = RegDeleteKeyTransacted(fixture, L"DeleteFixture", KEY_WOW64_64KEY, 0, transaction, NULL);
    fprintf(log, "SeparateDelete=%ld\n", error);
    check(!error, "delete previously committed key");
    check(CommitTransaction(transaction), "commit deletion");
    CloseHandle(transaction);
    error = RegOpenKeyEx(fixture, L"DeleteFixture", 0, KEY_READ | KEY_WOW64_64KEY, &key);
    fprintf(log, "SeparateDeleteOpen=%ld\n", error);
    check(error == ERROR_FILE_NOT_FOUND, "previously committed key removed");
    if (!error) { RegCloseKey(key); key = NULL; }
    // Also check creation/deletion in a single transaction used by CHECKPRESERVE.
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create same transaction"); return; }
    error = RegCreateKeyTransacted(fixture, L"DeleteFixture", 0, NULL, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL, transaction, NULL);
    check(!error, "create same-transaction fixture");
    if (!error) { RegCloseKey(key); key = NULL; }
    error = RegDeleteKeyTransacted(fixture, L"DeleteFixture", KEY_WOW64_64KEY, 0, transaction, NULL);
    fprintf(log, "SameDelete=%ld\n", error);
    check(!error, "delete same-transaction fixture");
    error = RegOpenKeyTransacted(fixture, L"DeleteFixture", 0,
        KEY_READ | KEY_WOW64_64KEY, &key, transaction, NULL);
    fprintf(log, "SameDeleteOpenBeforeCommit=%ld\n", error);
    if (!error) { RegCloseKey(key); key = NULL; }
    check(CommitTransaction(transaction), "commit same-transaction deletion");
    CloseHandle(transaction);
    error = RegOpenKeyEx(fixture, L"DeleteFixture", 0, KEY_READ | KEY_WOW64_64KEY, &key);
    fprintf(log, "SameDeleteOpenAfterCommit=%ld\n", error);
    if (!error) {
        DWORD children = 0, values = 0;
        LONG query = RegQueryInfoKey(key, NULL, NULL, NULL, &children, NULL, NULL,
            &values, NULL, NULL, NULL, NULL);
        check(!query && !children && !values, "Vista same-transaction deletion leaves only an empty key");
        RegCloseKey(key);
    } else check(error == ERROR_FILE_NOT_FOUND, "same-transaction key removed after commit");
}
static void removeViews(VOID)
{
    HKEY a = NULL;
    HANDLE transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    LONG error;
    unsigned i;
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create explicit removal transaction"); return; }
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyTransacted(fixture, sources[i], 0, KEY_ALL_ACCESS, &a, transaction, NULL);
        if (error) { check(FALSE, "open removal view"); break; }
        error = KxCfgpPreserveIfeoView(a, NULL, transaction, 0);
        check(!error, "disable view without creating backup");
        check(missing(a, L"same.exe", L"KEX_WinVerSpoof") &&
            missing(a, L"filtered.exe\\FilterA", L"KEX_WinVerSpoof") &&
            missing(a, L"{VxKexPropagationVirtualKey}", L"KEX_WinVerSpoof"), "removal includes user profiles and internal template");
        check(hasDword(a, L"same.exe", L"ForeignValue", 123) &&
            hasDword(a, L"same.exe", L"GlobalFlag", 2) &&
            hasText(a, L"foreign.exe", L"Debugger", L"foreign-debugger.exe"), "removal keeps unowned values and flags");
        RegCloseKey(a); a = NULL;
    }
    if (a) RegCloseKey(a);
    check(RollbackTransaction(transaction), "rollback explicit removal");
    CloseHandle(transaction);
    for (i = 0; i < 2; ++i) check(hasDword(source[i], L"same.exe", L"KEX_WinVerSpoof", 100 + i) &&
        hasDword(source[i], L"{VxKexPropagationVirtualKey}", L"KEX_WinVerSpoof", 400 + i), "removal rollback restores owned profiles");
}
VOID __cdecl mainCRTStartup(VOID)
{
    PCWSTR path = L"Software\\VxKexPreserveViewFixture-20261001-" SUFFIX;
    DWORD disposition;
    HKEY key;
    unsigned i;
    LONG error;
    log = fopen(LOGFILE, "wt");
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    error = RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS,
        NULL, &fixture, &disposition);
    if (error || disposition != REG_CREATED_NEW_KEY) {
        fprintf(log, "Fixture unavailable/existing=%ld\n", error);
        if (!error) RegCloseKey(fixture);
        fclose(log); ExitProcess(3);
    }
    for (i = 0; i < 2; ++i) {
        source[i] = create(fixture, sources[i]);
        backup[i] = create(fixture, backups[i]);
        if (!source[i] || !backup[i]) goto Cleanup;
        configure(source[i], L"same.exe", 100 + i);
        key = create(source[i], L"same.exe");
        dword(key, L"ForeignValue", 123); RegCloseKey(key);
        configure(source[i], L"old-basename.exe", 200 + i);
        key = create(source[i], L"filtered.exe");
        dword(key, L"UseFilter", 1); RegCloseKey(key);
        configure(source[i], L"filtered.exe\\FilterA", 300 + i);
        key = create(source[i], L"filtered.exe\\FilterA");
        text(key, L"FilterFullPath", L"C:\\Gone\\filtered.exe"); RegCloseKey(key);
        key = create(source[i], L"foreign.exe");
        text(key, L"VerifierDlls", L"prefixkexdll.dll kexdll.dll.backup Other.DLL");
        text(key, L"Debugger", L"foreign-debugger.exe"); RegCloseKey(key);
        configure(source[i], L"{VxKexPropagationVirtualKey}", 400 + i);
    }
    // Force failure only after the first view has already been processed.
    {
        UNICODE_STRING name;
        BYTE bad[] = {1, 2, 3};
        key = create(source[1], L"same.exe");
        RtlInitUnicodeString(&name, L"KEX_BrokenDword");
        check(NT_SUCCESS(NtSetValueKey(key, &name, 0, REG_DWORD, bad, sizeof(bad))), "create corrupt second view");
        RegCloseKey(key);
    }
    check(processViews(FALSE, TRUE) == ERROR_INVALID_DATA, "second view failure rejects complete snapshot");
    check(hasDword(source[0], L"same.exe", L"KEX_WinVerSpoof", 100) &&
        hasDword(source[1], L"same.exe", L"KEX_WinVerSpoof", 101), "failure rolls both views back");
    check(missing(backup[0], L"", L"StoreVersion") &&
        missing(backup[1], L"", L"StoreVersion"), "failed snapshot leaves no valid store");
    key = create(source[1], L"same.exe");
    check(!RegDeleteValue(key, L"KEX_BrokenDword"), "remove corrupt fixture value"); RegCloseKey(key);
    check(!processViews(FALSE, TRUE), "commit both view snapshots");
    for (i = 0; i < 2; ++i) {
        check(hasDword(backup[i], L"", L"RecordCount", 3), "only owned profiles saved");
        check(missing(source[i], L"same.exe", L"KEX_WinVerSpoof") &&
            missing(source[i], L"old-basename.exe", L"KEX_WinVerSpoof") &&
            missing(source[i], L"filtered.exe\\FilterA", L"KEX_WinVerSpoof"), "all owned profiles disabled");
        check(hasDword(source[i], L"same.exe", L"ForeignValue", 123) &&
            hasText(source[i], L"foreign.exe", L"Debugger", L"foreign-debugger.exe"), "foreign settings kept");
        check(hasDword(source[i], L"{VxKexPropagationVirtualKey}", L"KEX_WinVerSpoof", 400 + i), "internal template excluded");
        check(!RegDeleteTree(source[i], L"filtered.exe"), "remove filter to test identity reconstruction");
        check(!RegDeleteTree(source[i], L"old-basename.exe"), "remove basename key to test reconstruction");
    }
    key = create(source[1], L"same.exe");
    dword(key, L"KEX_WinVerSpoof", 999); RegCloseKey(key);
    check(processViews(TRUE, TRUE) == ERROR_ALREADY_EXISTS, "second view restore conflict rejected");
    check(missing(source[0], L"same.exe", L"KEX_WinVerSpoof") &&
        missing(source[0], L"filtered.exe\\FilterA", L"KEX_WinVerSpoof"), "restore failure rolls earlier view back");
    check(hasDword(source[1], L"same.exe", L"KEX_WinVerSpoof", 999) &&
        hasDword(backup[0], L"", L"RecordCount", 3), "conflict keeps new settings and saved store");
    key = create(source[1], L"same.exe");
    check(!RegDeleteValue(key, L"KEX_WinVerSpoof"), "remove fixture conflict"); RegCloseKey(key);
    check(!processViews(TRUE, TRUE), "restore both view snapshots");
    for (i = 0; i < 2; ++i) {
        check(hasDword(source[i], L"same.exe", L"KEX_WinVerSpoof", 100 + i) &&
            hasDword(source[i], L"old-basename.exe", L"KEX_WinVerSpoof", 200 + i) &&
            hasDword(source[i], L"filtered.exe\\FilterA", L"KEX_WinVerSpoof", 300 + i), "same basename retains separate view settings");
        check(hasText(source[i], L"filtered.exe\\FilterA", L"FilterFullPath", L"C:\\Gone\\filtered.exe") &&
            hasDword(source[i], L"filtered.exe", L"UseFilter", 1), "filter identity reconstructed");
        check(hasDword(backup[i], L"", L"RecordCount", 3), "successful restore retains store until caller consumes");
    }
    nativeViews();
    deleteTransactions();
    removeViews();
    dword(backup[0], L"RecordCount", 2);
    check(processViews(TRUE, TRUE) == ERROR_INVALID_DATA, "record count mismatch rejected");
    dword(backup[0], L"RecordCount", 3);
    key = create(backup[0], L"00000000");
    text(key, L"IfeoRelativePath", L"same.exe\\..\\extra"); RegCloseKey(key);
    check(processViews(TRUE, TRUE) == ERROR_INVALID_DATA, "malformed relative identity rejected");
    check(hasDword(source[0], L"same.exe", L"KEX_WinVerSpoof", 100), "invalid store leaves source unchanged");
    // The store is not consumed on invalid input; clear this fixture explicitly.
    for (i = 0; i < 2; ++i) check(!RegDeleteTree(backup[i], NULL), "clear fixture store");
    key = create(source[1], L"foreign.exe");
    text(key, L"Debugger", L"\"C:\\Legacy\\VxKexLdr.exe\" -legacy"); RegCloseKey(key);
    check(processViews(FALSE, TRUE) == ERROR_NOT_SUPPORTED, "unsupported legacy loader prevents incomplete snapshot");
    check(hasDword(source[0], L"same.exe", L"KEX_WinVerSpoof", 100) &&
        hasDword(source[1], L"same.exe", L"KEX_WinVerSpoof", 101), "legacy refusal rolls all views back");
    check(missing(backup[0], L"", L"StoreVersion") && missing(backup[1], L"", L"StoreVersion"), "legacy refusal leaves no valid store");
Cleanup:
    for (i = 0; i < 2; ++i) {
        if (source[i]) RegCloseKey(source[i]);
        if (backup[i]) RegCloseKey(backup[i]);
    }
    check(!RegDeleteTree(fixture, NULL), "clean fixture contents");
    RegCloseKey(fixture);
    check(!RegDeleteKey(HKEY_CURRENT_USER, path), "clean fixture root");
    fprintf(log, "Failures=%d\n", failures);
    fclose(log); ExitProcess(failures ? 1 : 0);
}
