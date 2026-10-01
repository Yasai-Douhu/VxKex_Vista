#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-all-x64.txt"
#else
#define LOGFILE "C:\\VxKexProbe\\NextParity\\preserve-all-x86.txt"
#endif
#define IFEO L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"
#define STORE L"Software\\VXsoft\\VxKexVistaPreserved"
static FILE *log;
static int failures;
static void check(BOOL ok, PCSTR message)
{
    fprintf(log, "%s %s\n", ok ? "PASS" : "FAIL", message);
    if (!ok) ++failures;
}
static void hashBytes(PULONGLONG hash, PCBYTE data, DWORD size)
{
    DWORD i;
    for (i = 0; i < size; ++i) { *hash ^= data[i]; *hash *= 1099511628211ui64; }
}
static LONG fingerprint(HKEY root, REGSAM view, PULONGLONG hash, unsigned depth)
{
    DWORD i, chars, bytes, type;
    WCHAR name[256];
    BYTE *data;
    HKEY child;
    LONG error = 0;
    if (depth > 32) return ERROR_INVALID_DATA;
    data = HeapAlloc(GetProcessHeap(), 0, 1024 * 1024);
    if (!data) return ERROR_NOT_ENOUGH_MEMORY;
    for (i = 0;; ++i) {
        chars = ARRAYSIZE(name); bytes = 1024 * 1024;
        error = RegEnumValue(root, i, name, &chars, NULL, &type, data, &bytes);
        if (error == ERROR_NO_MORE_ITEMS) { error = 0; break; }
        if (error) goto Done;
        hashBytes(hash, (PCBYTE)name, (chars + 1) * 2);
        hashBytes(hash, (PCBYTE)&type, sizeof(type));
        hashBytes(hash, (PCBYTE)&bytes, sizeof(bytes));
        hashBytes(hash, data, bytes);
    }
    for (i = 0;; ++i) {
        chars = ARRAYSIZE(name);
        error = RegEnumKeyEx(root, i, name, &chars, NULL, NULL, NULL, NULL);
        if (error == ERROR_NO_MORE_ITEMS) { error = 0; break; }
        if (error) goto Done;
        hashBytes(hash, (PCBYTE)name, (chars + 1) * 2);
        error = RegOpenKeyEx(root, name, 0, KEY_READ | view, &child);
        if (error) goto Done;
        error = fingerprint(child, view, hash, depth + 1);
        RegCloseKey(child);
        if (error) goto Done;
    }
Done:
    HeapFree(GetProcessHeap(), 0, data);
    return error;
}
static BOOL restrictedStore(HKEY key, BOOL protectedRoot)
{
    BYTE buffer[4096], sidSystem[SECURITY_MAX_SID_SIZE], sidAdmin[SECURITY_MAX_SID_SIZE];
    DWORD bytes = sizeof(buffer), revision, sidBytes;
    SECURITY_DESCRIPTOR_CONTROL control;
    PACL acl;
    BOOL present, defaulted, systemFound = FALSE, adminFound = FALSE;
    unsigned i;
    if (RegGetKeySecurity(key, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buffer, &bytes)) return FALSE;
    if (!GetSecurityDescriptorControl(buffer, &control, &revision) || (protectedRoot && !(control & SE_DACL_PROTECTED)) ||
        !GetSecurityDescriptorDacl(buffer, &present, &acl, &defaulted) || !present || !acl || acl->AceCount != 2) return FALSE;
    sidBytes = sizeof(sidSystem);
    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, sidSystem, &sidBytes)) return FALSE;
    sidBytes = sizeof(sidAdmin);
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sidAdmin, &sidBytes)) return FALSE;
    for (i = 0; i < acl->AceCount; ++i) {
        PACCESS_ALLOWED_ACE ace;
        if (!GetAce(acl, i, (PVOID *)&ace) || ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE ||
            (ace->Mask & KEY_ALL_ACCESS) != KEY_ALL_ACCESS) return FALSE;
        if (EqualSid(&ace->SidStart, sidSystem)) systemFound = TRUE;
        else if (EqualSid(&ace->SidStart, sidAdmin)) adminFound = TRUE;
        else return FALSE;
    }
    return systemFound && adminFound;
}
static void uninstallRollback(BOOLEAN keep)
{
    HANDLE transaction;
    HKEY root = NULL, template = NULL;
    REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    ULONGLONG before[2], after;
    WCHAR verifiers[256];
    DWORD bytes, type;
    LONG error;
    unsigned i;
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IFEO, 0, KEY_READ | views[i], &root);
        if (error) { check(FALSE, "open baseline uninstall view"); return; }
        before[i] = 14695981039346656037ui64;
        check(!fingerprint(root, views[i], &before[i], 0), "fingerprint uninstall baseline");
        RegCloseKey(root); root = NULL;
    }
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { check(FALSE, "create uninstall test transaction"); return; }
    check(KxCfgPrepareUninstall(keep, transaction), keep ? "prepare uninstall retaining settings" : "prepare uninstall removing settings");
    fprintf(log, "UninstallKeep=%u Error=%lu\n", keep, GetLastError());
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO L"\\{VxKexPropagationVirtualKey}", 0,
            KEY_READ | views[i], &template, transaction, NULL);
        if (error == ERROR_FILE_NOT_FOUND) continue;
        if (error) { check(FALSE, "open disabled internal template"); continue; }
        bytes = sizeof(verifiers);
        error = RegQueryValueEx(template, L"VerifierDlls", NULL, &type, (PBYTE)verifiers, &bytes);
        check(error == ERROR_FILE_NOT_FOUND || (!error && type == REG_SZ &&
            bytes >= 2 && bytes <= sizeof(verifiers) && !(bytes & 1) && !verifiers[bytes / 2 - 1] &&
            !KxCfgpRemoveKexDllFromVerifierDlls(verifiers)), "propagation template no longer loads KexDll");
        RegCloseKey(template); template = NULL;
    }
    if (keep) check(KxCfgRestoreAllConfigurations(transaction), "retained user settings can be restored");
    check(RollbackTransaction(transaction), "rollback uninstall test");
    CloseHandle(transaction);
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IFEO, 0, KEY_READ | views[i], &root);
        if (error) { check(FALSE, "open uninstall result view"); continue; }
        after = 14695981039346656037ui64;
        check(!fingerprint(root, views[i], &after, 0) && after == before[i], "uninstall rollback leaves complete IFEO state unchanged");
        RegCloseKey(root); root = NULL;
    }
}
VOID __cdecl mainCRTStartup(VOID)
{
    HKEY roots[2] = {NULL, NULL}, store = NULL, viewStore = NULL;
    REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    PCWSTR names[] = {L"64", L"32"};
    ULONGLONG before[2], after;
    HANDLE transaction = NULL;
    DWORD type, bytes, count;
    LONG error;
    unsigned i;
    log = fopen(LOGFILE, "wt");
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    if (RtlOperatingSystemBitness() != 64) { check(FALSE, "probe requires Vista x64"); goto Done; }
    error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, STORE, 0, KEY_READ | KEY_WOW64_64KEY, &store);
    check(error == ERROR_FILE_NOT_FOUND, "no preexisting store; never overwrite it");
    if (error != ERROR_FILE_NOT_FOUND) goto Done;
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IFEO, 0, KEY_READ | views[i], &roots[i]);
        if (error) { check(FALSE, "open IFEO view"); goto Done; }
        before[i] = 14695981039346656037ui64;
        check(!fingerprint(roots[i], views[i], &before[i], 0), "fingerprint all IFEO values before test");
    }
    transaction = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (transaction == INVALID_HANDLE_VALUE) { transaction = NULL; check(FALSE, "create transaction"); goto Done; }
    check(KxCfgPreserveAllConfigurations(transaction), "preserve actual installed profiles in one transaction");
    fprintf(log, "PreserveError=%lu\n", GetLastError());
    if (failures) goto Done;
    error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, STORE, 0, KEY_READ | KEY_WOW64_64KEY, &store, transaction, NULL);
    if (error) { check(FALSE, "open staged store"); goto Done; }
    check(restrictedStore(store, TRUE), "store DACL grants only Administrators and SYSTEM");
    for (i = 0; i < 2; ++i) {
        error = RegOpenKeyTransacted(store, names[i], 0, KEY_READ | KEY_WOW64_64KEY, &viewStore, transaction, NULL);
        if (error) { check(FALSE, "open staged view"); goto Done; }
        bytes = 4;
        error = RegQueryValueEx(viewStore, L"RecordCount", NULL, &type, (PBYTE)&count, &bytes);
        check(!error && type == REG_DWORD && bytes == 4, "read saved profile count");
        fprintf(log, "SavedView=%ls Profiles=%lu\n", names[i], count);
        check(restrictedStore(viewStore, FALSE), "view store inherits restricted DACL");
        if (count) {
            HKEY configuration = NULL;
            error = RegOpenKeyTransacted(viewStore, L"00000000\\Configuration", 0,
                KEY_READ | KEY_WOW64_64KEY, &configuration, transaction, NULL);
            check(!error && restrictedStore(configuration, FALSE), "saved configuration inherits restricted DACL");
            if (!error) RegCloseKey(configuration);
        }
        RegCloseKey(viewStore); viewStore = NULL;
    }
    RegCloseKey(store); store = NULL;
    check(KxCfgRestoreAllConfigurations(transaction), "restore all installed profiles and consume staged store");
    fprintf(log, "RestoreError=%lu\n", GetLastError());
    error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, STORE, 0, KEY_READ | KEY_WOW64_64KEY, &store, transaction, NULL);
    fprintf(log, "ConsumedStoreOpen=%ld\n", error);
    if (!error) {
        DWORD children = 0, values = 0;
        LONG query = RegQueryInfoKey(store, NULL, NULL, NULL, &children, NULL, NULL,
            &values, NULL, NULL, NULL, NULL);
        fprintf(log, "ConsumedStoreQuery=%ld Children=%lu Values=%lu\n", query, children, values);
        // Vista may keep an empty key created and deleted in one transaction.
        // This dry run is rolled back; real reinstall deletes a committed store.
        check(!query && !children && !values, "no saved records remain in same-transaction dry run");
    } else {
        check(error == ERROR_FILE_NOT_FOUND || error == ERROR_KEY_DELETED,
            "successful restore consumes store in transaction");
    }
Done:
    if (viewStore) RegCloseKey(viewStore);
    if (store) RegCloseKey(store);
    if (transaction) {
        check(RollbackTransaction(transaction), "rollback whole test");
        CloseHandle(transaction);
        for (i = 0; i < 2; ++i) {
            after = 14695981039346656037ui64;
            error = fingerprint(roots[i], views[i], &after, 0);
            fprintf(log, "IFEOView=%u Before=%016I64x After=%016I64x\n", i, before[i], after);
            check(!error && after == before[i], "all IFEO values unchanged after rollback");
        }
        store = NULL;
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, STORE, 0, KEY_READ | KEY_WOW64_64KEY, &store);
        check(error == ERROR_FILE_NOT_FOUND, "no saved store left by test");
        if (!error) RegCloseKey(store);
    }
    for (i = 0; i < 2; ++i) if (roots[i]) RegCloseKey(roots[i]);
    if (!failures) { uninstallRollback(TRUE); uninstallRollback(FALSE); }
    fprintf(log, "Failures=%d\n", failures);
    fclose(log); ExitProcess(failures ? 1 : 0);
}
