#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-result-x64.txt"
#else
#define SUFFIX L"x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-result-x86.txt"
#endif
static FILE *log;
static int failures;
static HKEY fixture, source, backup;
static PCWSTR command = L"\"C:\\VxKex\\VistaRun.exe\" --ifeo";
static void check(BOOL success, PCSTR name)
{
    fprintf(log, "%s %s error=%lu\n", success ? "PASS" : "FAIL", name, GetLastError());
    if (!success) ++failures;
}
static void putDword(HKEY key, PCWSTR name, DWORD value)
{
    check(!RegSetValueEx(key, name, 0, REG_DWORD, (PCBYTE)&value, 4), "write fixture DWORD");
}
static void putText(HKEY key, PCWSTR name, PCWSTR value)
{
    check(!RegSetValueEx(key, name, 0, REG_SZ, (PCBYTE)value,
        (DWORD)((wcslen(value) + 1) * 2)), "write fixture text");
}
static BOOL hasText(HKEY key, PCWSTR name, PCWSTR expected)
{
    WCHAR text[512];
    DWORD bytes = sizeof(text), type;
    LONG error = RegQueryValueEx(key, name, NULL, &type, (PBYTE)text, &bytes);
    if (!expected) return error == ERROR_FILE_NOT_FOUND;
    return !error && type == REG_SZ && bytes >= 2 && bytes <= sizeof(text) &&
        !(bytes & 1) && !text[bytes / 2 - 1] && !wcscmp(text, expected);
}
static BOOL hasDword(HKEY key, PCWSTR name, DWORD expected)
{
    DWORD bytes = 4, type, value;
    return !RegQueryValueEx(key, name, NULL, &type, (PBYTE)&value, &bytes) &&
        type == REG_DWORD && bytes == 4 && value == expected;
}
static BOOL hasOpaque(HKEY key)
{
    BYTE data[4], expected[] = {0, 0xff, 0x81, 0};
    DWORD bytes = sizeof(data), type;
    return !RegQueryValueEx(key, L"KEX_FutureOption", NULL, &type, data, &bytes) &&
        type == REG_BINARY && bytes == sizeof(expected) && !memcmp(data, expected, bytes);
}
static HANDLE openTransaction(HKEY *a, HKEY *b)
{
    HANDLE transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) return NULL;
    if (RegOpenKeyTransacted(fixture, L"Source", 0, KEY_ALL_ACCESS, a, transaction, NULL) ||
        RegOpenKeyTransacted(fixture, L"Backup", 0, KEY_ALL_ACCESS, b, transaction, NULL)) {
        RollbackTransaction(transaction); CloseHandle(transaction); return NULL;
    }
    return transaction;
}
static void finishTransaction(HANDLE transaction, HKEY a, HKEY b, BOOL commit)
{
    check(commit ? CommitTransaction(transaction) : RollbackTransaction(transaction), "finish transaction");
    RegCloseKey(a); RegCloseKey(b); CloseHandle(transaction);
}
VOID __cdecl mainCRTStartup(VOID)
{
    PCWSTR path = L"Software\\VxKexPreserveFixture-20261001-" SUFFIX;
    DWORD disposition, bytes;
    HKEY a = NULL, b = NULL;
    HANDLE transaction;
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
    if (RegCreateKeyEx(fixture, L"Source", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &source, NULL) ||
        RegCreateKeyEx(fixture, L"Backup", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &backup, NULL)) {
        check(FALSE, "create test keys"); goto Cleanup;
    }
    putDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin10);
    putText(source, L"KEX_ConfigPath", L"C:\\Fixture\\\x65e5\x672c\x8a9e.exe");
    putText(source, L"KEX_VistaDebugger", command);
    putText(source, L"Debugger", command);
    putText(source, L"VerifierDlls", L"Other.DLL kexdll.dll");
    putDword(source, L"GlobalFlag", 0x102);
    putDword(source, L"VerifierFlags", 0x80000000);
    putDword(source, L"ForeignValue", 123);
    {
        BYTE opaque[] = {0, 0xff, 0x81, 0};
        check(!RegSetValueEx(source, L"KEX_FutureOption", 0, REG_BINARY,
            opaque, sizeof(opaque)), "write future binary option");
    }
    transaction = openTransaction(&a, &b);
    check(transaction != NULL, "open snapshot transaction");
    if (!transaction) goto Cleanup;
    check(!KxCfgpPreserveIfeoConfiguration(a, b), "snapshot and disable");
    check(hasText(a, L"VerifierDlls", L"Other.DLL ") && hasText(a, L"Debugger", NULL), "disabled in transaction");
    finishTransaction(transaction, a, b, FALSE);
    check(hasText(source, L"Debugger", command) && hasDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin10), "snapshot rollback restores source");
    bytes = 0;
    check(RegQueryValueEx(backup, L"SavedVersion", NULL, NULL, NULL, &bytes) == ERROR_FILE_NOT_FOUND, "snapshot rollback leaves no backup");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open commit transaction"); goto Cleanup; }
    check(!KxCfgpPreserveIfeoConfiguration(a, b), "snapshot for commit");
    finishTransaction(transaction, a, b, TRUE);
    check(hasText(source, L"KEX_ConfigPath", NULL) && hasText(source, L"Debugger", NULL), "committed snapshot disables owned settings");
    check(hasOpaque(backup), "snapshot keeps unknown option type and bytes");
    check(hasDword(source, L"ForeignValue", 123) && hasDword(source, L"GlobalFlag", 0x102), "foreign values and flags preserved");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open existing backup transaction"); goto Cleanup; }
    check(KxCfgpPreserveIfeoConfiguration(a, b) == ERROR_ALREADY_EXISTS, "existing backup never overwritten");
    finishTransaction(transaction, a, b, FALSE);
    check(hasDword(backup, L"SavedEnabled", 1) && hasText(backup, L"SavedDebugger", command), "existing backup remains exact");
    putText(backup, L"SavedDebugger", L"unowned-command.exe");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open ownership conflict transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_INVALID_DATA, "unowned saved debugger rejected");
    finishTransaction(transaction, a, b, FALSE);
    check(hasText(source, L"Debugger", NULL), "unowned saved command never installed");
    putText(backup, L"SavedDebugger", command);
    putDword(source, L"VerifierFlags", 0x40000000);
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open shared verifier conflict transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_ALREADY_EXISTS, "changed shared verifier flags rejected");
    finishTransaction(transaction, a, b, FALSE);
    check(hasDword(source, L"VerifierFlags", 0x40000000) && hasText(source, L"VerifierDlls", L"Other.DLL "), "shared verifier conflict preserves provider");
    putDword(source, L"VerifierFlags", 0x80000000);
    putText(source, L"Debugger", L"foreign-debugger.exe");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open debugger conflict transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_ALREADY_EXISTS, "foreign debugger restore rejected");
    finishTransaction(transaction, a, b, FALSE);
    check(hasText(source, L"Debugger", L"foreign-debugger.exe") && hasDword(backup, L"SavedVersion", 1), "conflict retains debugger and backup");
    RegDeleteValue(source, L"Debugger");
    putDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin11);
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open settings conflict transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_ALREADY_EXISTS, "new settings restore rejected");
    finishTransaction(transaction, a, b, FALSE);
    check(hasDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin11), "new settings unchanged");
    RegDeleteValue(source, L"KEX_WinVerSpoof");
    putDword(backup, L"SavedVersion", 99);
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open unsupported version transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_INVALID_DATA, "unsupported backup version rejected");
    finishTransaction(transaction, a, b, FALSE);
    check(hasDword(backup, L"SavedVersion", 99) && hasText(source, L"Debugger", NULL), "unsupported backup retained");
    putDword(backup, L"SavedVersion", 1);
    RegDeleteValue(backup, L"SavedVerifierFlags");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open incomplete backup transaction"); goto Cleanup; }
    check(KxCfgpRestoreIfeoConfiguration(a, b) == ERROR_INVALID_DATA, "incomplete backup rejected");
    finishTransaction(transaction, a, b, FALSE);
    putDword(backup, L"SavedVerifierFlags", 0x80000000);
    {
        WCHAR unterminated[] = {L'x'};
        UNICODE_STRING valueName;
        DWORD storedType, storedBytes = 0;
        // RegSetValueEx adds a terminator on Vista; bypass that normalization.
        RtlInitUnicodeString(&valueName, L"KEX_BrokenText");
        check(NT_SUCCESS(NtSetValueKey(backup, &valueName, 0, REG_SZ,
            unterminated, sizeof(unterminated))), "write malformed string fixture");
        error = RegQueryValueEx(backup, L"KEX_BrokenText", NULL, &storedType, NULL, &storedBytes);
        fprintf(log, "Malformed string query=%ld type=%lu bytes=%lu\n", error, storedType, storedBytes);
        check(!error && storedType == REG_SZ && storedBytes == sizeof(unterminated), "malformed fixture remains unterminated");
        transaction = openTransaction(&a, &b);
        if (!transaction) { check(FALSE, "open corrupt backup transaction"); goto Cleanup; }
        error = KxCfgpRestoreIfeoConfiguration(a, b);
        fprintf(log, "Malformed string restore=%ld\n", error);
        check(error == ERROR_INVALID_DATA, "unterminated backup text rejected");
        finishTransaction(transaction, a, b, FALSE);
        check(hasText(source, L"KEX_ConfigPath", NULL), "corrupt restore rollback removes partial writes");
        RegDeleteValue(backup, L"KEX_BrokenText");
    }
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open restore transaction"); goto Cleanup; }
    check(!KxCfgpRestoreIfeoConfiguration(a, b), "restore preserved settings");
    finishTransaction(transaction, a, b, TRUE);
    check(hasText(source, L"Debugger", command) && hasText(source, L"KEX_ConfigPath", L"C:\\Fixture\\\x65e5\x672c\x8a9e.exe") &&
        hasDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin10), "restored configuration exact");
    check(hasText(source, L"VerifierDlls", L"Other.DLL kexdll.dll") && hasDword(source, L"ForeignValue", 123), "restored verifier preserves foreign provider and separator");
    check(hasDword(backup, L"SavedVersion", 1), "backup remains for caller to consume after success");
    check(hasOpaque(source), "restore keeps unknown option type and bytes");
    check(!RegDeleteTree(backup, NULL), "clear consumed fixture backup");
    putText(source, L"VerifierDlls", L"kexdll.dll");
    putDword(source, L"GlobalFlag", 2);
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open disabled snapshot transaction"); goto Cleanup; }
    check(!KxCfgpPreserveIfeoConfiguration(a, b), "snapshot disabled configuration");
    finishTransaction(transaction, a, b, TRUE);
    check(hasDword(backup, L"SavedEnabled", 0) && hasDword(source, L"GlobalFlag", 2), "disabled state captured and unrelated flags preserved");
    transaction = openTransaction(&a, &b);
    if (!transaction) { check(FALSE, "open disabled restore transaction"); goto Cleanup; }
    check(!KxCfgpRestoreIfeoConfiguration(a, b), "restore disabled configuration");
    finishTransaction(transaction, a, b, TRUE);
    check(hasText(source, L"VerifierDlls", NULL) && hasDword(source, L"GlobalFlag", 2) &&
        hasDword(source, L"KEX_WinVerSpoof", WinVerSpoofWin10), "restore does not enable inactive verifier");
Cleanup:
    if (source) RegCloseKey(source);
    if (backup) RegCloseKey(backup);
    check(!RegDeleteTree(fixture, NULL), "clean fixture contents");
    RegCloseKey(fixture);
    check(!RegDeleteKey(HKEY_CURRENT_USER, path), "clean fixture root");
    fprintf(log, "Failures=%d\n", failures);
    fclose(log); ExitProcess(failures ? 1 : 0);
}
