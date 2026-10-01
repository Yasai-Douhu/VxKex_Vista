#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <sddl.h>
#include <Aclapi.h>
#include <stdio.h>
#define FIXTURE L"Software\\VxKexParityLogging20261001"
#define PRODUCT L"Software\\VXsoft\\VxKex"
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR message)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", message, GetLastError());
    fflush(log); if (!ok) ++failures;
}
static BOOL present(HKEY root)
{
    HKEY key; LONG error = RegOpenKeyEx(root, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!error) RegCloseKey(key); return !error;
}
static void query(BOOLEAN expected, PCWSTR path, PCSTR message)
{
    BOOLEAN enabled = !expected; WCHAR actual[MAX_PATH];
    check(KxCfgQueryLoggingSettings(&enabled, actual, ARRAYSIZE(actual)) && enabled == expected && !wcscmp(actual, path), message);
}
VOID __cdecl mainCRTStartup(VOID)
{
    HKEY base = NULL, machine = NULL, user = NULL, key = NULL, temporary;
    HKEY selectedSoftware = NULL, machineSoftware = NULL;
    HANDLE tx; BOOLEAN enabled; WCHAR path[MAX_PATH], oversized[MAX_PATH + 5];
    LONG error; DWORD disposition, index; PSECURITY_DESCRIPTOR descriptor = NULL;
    BOOL overriddenMachine = FALSE, overriddenUser = FALSE;
#ifdef _WIN64
    log = fopen("C:\\VxKexProbe\\NextParity\\logging-settings-x64.txt", "wt");
#else
    log = fopen("C:\\VxKexProbe\\NextParity\\logging-settings-x86.txt", "wt");
#endif
    if (!log) ExitProcess(2);
    error = RegCreateKeyEx(HKEY_CURRENT_USER, FIXTURE, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &base, &disposition);
    check(!error && disposition == REG_CREATED_NEW_KEY, "new dedicated fixture only");
    if (error || disposition != REG_CREATED_NEW_KEY) goto Done;
    check(!RegCreateKeyEx(base, L"Machine", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &machine, NULL) &&
        !RegCreateKeyEx(base, L"User", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &user, NULL), "create isolated machine and user roots");
    if (failures) goto Cleanup;
    overriddenMachine = !RegOverridePredefKey(HKEY_LOCAL_MACHINE, machine);
    overriddenUser = !RegOverridePredefKey(HKEY_CURRENT_USER, user);
    check(overriddenMachine && overriddenUser, "redirect predefined roots in this diagnostic process only"); if (failures) goto Cleanup;
    query(FALSE, L"", "missing configuration has deterministic disabled and empty defaults");
    check(!present(machine) && !present(user), "query does not create either missing product key");
    check(!RegCreateKeyEx(machine, PRODUCT, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL), "create machine defaults");
    if (failures) goto Cleanup;
    check(!RegWriteI32(key, NULL, L"EnableLogging", 1) && !RegWriteString(key, NULL, L"LogDir", L"C:\\MachineLogs"), "write machine defaults");
    check(!RegWriteString(key, NULL, L"KexDir", L"C:\\VxKex"), "set installed directory in isolated machine configuration");
    RegCloseKey(key); key = NULL;
    query(TRUE, L"C:\\MachineLogs", "missing user key inherits actual machine configuration");
    check(!present(user), "machine-only query leaves user hive unchanged");
    check(!RegCreateKeyEx(user, PRODUCT, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL), "create user override fixture");
    check(!RegWriteI32(key, NULL, L"EnableLogging", 0), "set only user logging toggle");
    query(FALSE, L"C:\\MachineLogs", "user toggle overrides without erasing inherited directory");
    check(!RegWriteString(key, NULL, L"LogDir", L"C:\\UserLogs"), "set user directory");
    query(FALSE, L"C:\\UserLogs", "user directory overrides machine directory");
    check(!RegDeleteValue(key, L"EnableLogging"), "remove optional user toggle");
    query(TRUE, L"C:\\UserLogs", "missing user toggle inherits instead of reading uninitialized data");
    check(KxCfgQueryLoggingSettings(&enabled, NULL, 0) && enabled, "optional directory output");
    check(KxCfgQueryLoggingSettings(NULL, path, ARRAYSIZE(path)) && !wcscmp(path, L"C:\\UserLogs"), "optional toggle output");
    check(!KxCfgQueryLoggingSettings(&enabled, path, 2) && GetLastError() == ERROR_MORE_DATA && !path[0], "small output reports failure and no truncated path");
    check(!KxCfgQueryLoggingSettings(&enabled, NULL, 1) && GetLastError() == ERROR_INVALID_PARAMETER, "invalid buffer pair rejected");
    check(!RegWriteString(key, NULL, L"EnableLogging", L"not-a-dword"), "create malformed toggle");
    check(!KxCfgQueryLoggingSettings(&enabled, path, ARRAYSIZE(path)) && !enabled && !path[0], "invalid value type fails without inventing configuration");
    check(!RegDeleteValue(key, L"EnableLogging"), "restore valid user fixture");
    check(ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:P(A;;KR;;;WD)", SDDL_REVISION_1, &descriptor, NULL), "create read-only user-key ACL");
    if (descriptor) {
        check(!RegSetKeySecurity(key, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor), "deny writing while permitting reading");
        error = RegOpenKeyEx(user, PRODUCT, 0, KEY_WRITE | KEY_WOW64_64KEY, &temporary);
        check(error == ERROR_ACCESS_DENIED, "read-only fixture actually rejects write-open"); if (!error) RegCloseKey(temporary);
        query(TRUE, L"C:\\UserLogs", "query succeeds against key with no write access");
        LocalFree(descriptor); descriptor = NULL;
        check(ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:P(A;;KA;;;WD)", SDDL_REVISION_1, &descriptor, NULL), "prepare fixture ACL restoration");
        if (descriptor) check(!RegSetKeySecurity(key, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor), "restore test-owned key ACL using existing authorized handle");
    }
    RegCloseKey(key); key = NULL;
    check(!RegDeleteTree(user, PRODUCT), "remove user fixture before save transaction");
    query(TRUE, L"C:\\MachineLogs", "query remains read-only before first explicit save");
    tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    check(tx != INVALID_HANDLE_VALUE, "create first-save transaction");
    if (tx != INVALID_HANDLE_VALUE) {
        check(KxCfgConfigureLoggingSettings(FALSE, L"C:\\NewUserLogs", tx), "explicit save creates missing user key transactionally");
        check(!present(user), "new user preference remains invisible before commit");
        check(RollbackTransaction(tx), "rollback first save"); CloseHandle(tx);
        check(!present(user), "rollback removes all new user preference keys");
    }
    for (index = 0; index < ARRAYSIZE(oversized) - 1; ++index) oversized[index] = L'x';
    oversized[ARRAYSIZE(oversized) - 1] = 0;
    check(!KxCfgConfigureLoggingSettings(TRUE, oversized, NULL) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && !present(user),
        "oversized expanded directory rejected before any preference write");
    tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    check(tx != INVALID_HANDLE_VALUE, "create committed save transaction");
    if (tx != INVALID_HANDLE_VALUE) {
        check(KxCfgConfigureLoggingSettings(FALSE, L"C:\\CommittedLogs", tx), "stage explicit user preferences and cleanup registration");
        check(CommitTransaction(tx), "commit preferences"); CloseHandle(tx);
        query(FALSE, L"C:\\CommittedLogs", "committed user preferences are effective");
        error = RegOpenKeyEx(machine, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files",
            0, KEY_READ | KEY_WOW64_64KEY, &temporary);
        check(!error, "open committed cleanup registration in isolated machine root");
        if (!error) {
            check(!RegReadString(temporary, NULL, L"Folder", path, ARRAYSIZE(path)) && !wcscmp(path, L"C:\\CommittedLogs"),
                "cleanup registration uses staged path rather than previous committed defaults");
            RegCloseKey(temporary);
        }
    }
    check(!RegCreateKeyEx(base, L"SelectedUserSoftware", 0, NULL, 0, KEY_ALL_ACCESS,
        NULL, &selectedSoftware, NULL), "create separate initiating-user Software root");
    check(!RegOpenKeyEx(machine, L"Software", 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &machineSoftware),
        "open explicit isolated machine Software root");
    if (selectedSoftware && machineSoftware) {
        tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
        check(tx != INVALID_HANDLE_VALUE, "create different-user save transaction");
        if (tx != INVALID_HANDLE_VALUE) {
            error = KxCfgpStageLoggingSettings(selectedSoftware, machineSoftware, L"C:\\VxKex", TRUE, L"C:\\SelectedUserLogs", tx);
            check(!error, "stage logging to explicitly selected initiating-user root");
            check(error ? RollbackTransaction(tx) : CommitTransaction(tx), "finish different-user save transaction"); CloseHandle(tx);
        }
        query(FALSE, L"C:\\CommittedLogs", "different-user save leaves operator HKCU preferences untouched");
        error = RegOpenKeyEx(selectedSoftware, L"VXsoft\\VxKex", 0, KEY_READ, &temporary);
        check(!error, "open selected initiating-user preferences");
        if (!error) {
            DWORD toggle;
            check(!RegReadI32(temporary, NULL, L"EnableLogging", &toggle) && toggle == 1 &&
                !RegReadString(temporary, NULL, L"LogDir", path, ARRAYSIZE(path)) && !wcscmp(path, L"C:\\SelectedUserLogs"),
                "only selected user receives requested settings"); RegCloseKey(temporary);
        }
        tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
        if (tx != INVALID_HANDLE_VALUE) {
            check(!KxCfgpStageLoggingSettings(selectedSoftware, machineSoftware, L"C:\\VxKex", FALSE, L"C:\\RolledBackSelectedLogs", tx),
                "stage second selected-user preference");
            check(RollbackTransaction(tx), "rollback selected-user save"); CloseHandle(tx);
        } else check(FALSE, "create selected-user rollback transaction");
        error = RegOpenKeyEx(machineSoftware, L"Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files",
            0, KEY_ALL_ACCESS, &temporary);
        check(!error, "open cleanup registration for conflict and preservation tests");
        if (!error) {
            check(!RegReadString(temporary, NULL, L"Folder", path, ARRAYSIZE(path)) && !wcscmp(path, L"C:\\SelectedUserLogs"),
                "selected-user rollback also restores cleanup folder");
            check(!RegWriteString(temporary, NULL, NULL, L"{11111111-1111-1111-1111-111111111111}"), "create foreign cleanup registration owner");
            RegCloseKey(temporary);
        }
        tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
        if (tx != INVALID_HANDLE_VALUE) {
            check(KxCfgpStageLoggingSettings(selectedSoftware, machineSoftware, L"C:\\VxKex", FALSE, L"C:\\ConflictLogs", tx) == ERROR_ALREADY_EXISTS,
                "foreign cleanup registration is rejected instead of overwritten");
            check(RollbackTransaction(tx), "rollback cleanup conflict"); CloseHandle(tx);
        } else check(FALSE, "create cleanup conflict transaction");
        error = RegOpenKeyEx(selectedSoftware, L"VXsoft\\VxKex", 0, KEY_READ, &temporary);
        if (!error) {
            check(!RegReadString(temporary, NULL, L"LogDir", path, ARRAYSIZE(path)) && !wcscmp(path, L"C:\\SelectedUserLogs"),
                "cleanup conflict leaves initiating-user settings unchanged"); RegCloseKey(temporary);
        } else check(FALSE, "read selected user after conflict");
        query(FALSE, L"C:\\CommittedLogs", "operator remains unchanged after selected-user rollback and conflict");
    }
Cleanup:
    if (selectedSoftware) RegCloseKey(selectedSoftware); if (machineSoftware) RegCloseKey(machineSoftware);
    if (descriptor) LocalFree(descriptor);
    if (key) RegCloseKey(key);
    if (overriddenUser) RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
    if (overriddenMachine) RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL);
    if (user) RegCloseKey(user); if (machine) RegCloseKey(machine);
    check(!RegDeleteTree(base, NULL), "remove isolated fixture contents");
    RegCloseKey(base); base = NULL;
    check(!RegDeleteKeyEx(HKEY_CURRENT_USER, FIXTURE, KEY_WOW64_64KEY, 0), "remove owned fixture root");
Done:
    if (base) RegCloseKey(base);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
