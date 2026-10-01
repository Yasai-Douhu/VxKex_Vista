#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <sddl.h>
#include <Aclapi.h>
#include <stdio.h>
#define TREE L"C:\\VxKexProbe\\NextParity\\SetupCacheFixture"
#define PACKAGE L"C:\\VxKexProbe\\NextParity\\RealPackage"
DWORD VistaSetupStagePackage(PCWSTR Package, PCWSTR CacheRoot, PWSTR Staged, DWORD StagedChars);
DWORD VistaSetupCollectCache(PCWSTR CacheRoot, PCWSTR KeepPackage);
DWORD VistaSetupCompleteCache(PCWSTR CacheRoot, PCWSTR Package, HANDLE OwnerProcess);
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR text) { fprintf(log, "%s %s\n", ok ? "PASS" : "FAIL", text); if (!ok) ++failures; }
static BOOL security(PCWSTR path)
{
    PSECURITY_DESCRIPTOR sd = NULL; PSID owner; PACL acl; DWORD error, size, i, revision;
    BYTE admin[SECURITY_MAX_SID_SIZE], system[SECURITY_MAX_SID_SIZE]; SECURITY_DESCRIPTOR_CONTROL control;
    BOOL a = FALSE, s = FALSE, ok;
    size = sizeof(admin); if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, admin, &size)) return FALSE;
    size = sizeof(system); if (!CreateWellKnownSid(WinLocalSystemSid, NULL, system, &size)) return FALSE;
    error = GetNamedSecurityInfo((PWSTR)path, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, NULL, &acl, NULL, &sd);
    if (error) return FALSE;
    ok = owner && EqualSid(owner, admin) && acl && acl->AceCount == 2 &&
        GetSecurityDescriptorControl(sd, &control, &revision) && (control & SE_DACL_PROTECTED);
    for (i = 0; ok && i < acl->AceCount; ++i) {
        PACCESS_ALLOWED_ACE ace;
        if (!GetAce(acl, i, (PVOID *)&ace) || ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE ||
            (ace->Mask & FILE_ALL_ACCESS) != FILE_ALL_ACCESS) { ok = FALSE; break; }
        if (EqualSid(&ace->SidStart, admin)) a = TRUE;
        else if (EqualSid(&ace->SidStart, system)) s = TRUE;
        else ok = FALSE;
    }
    LocalFree(sd); return ok && a && s;
}
static unsigned packages(void)
{
    WIN32_FIND_DATA find; HANDLE h = FindFirstFile(TREE L"\\Cache\\Package-*", &find); unsigned count = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do { ++count; } while (FindNextFile(h, &find)); FindClose(h); return count;
}
static BOOL securityTree(PCWSTR path)
{
    WCHAR child[MAX_PATH]; WIN32_FIND_DATA find; HANDLE search; BOOL ok = security(path);
    if (!ok || !(GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY)) return ok;
    if (FAILED(StringCchPrintf(child, ARRAYSIZE(child), L"%s\\*", path))) return FALSE;
    search = FindFirstFile(child, &find); if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        if (!wcscmp(find.cFileName, L".") || !wcscmp(find.cFileName, L"..")) continue;
        if (FAILED(StringCchPrintf(child, ARRAYSIZE(child), L"%s\\%s", path, find.cFileName)) || !securityTree(child)) { ok = FALSE; break; }
    } while (FindNextFile(search, &find)); FindClose(search); return ok;
}
VOID __cdecl mainCRTStartup(VOID)
{
    WCHAR first[MAX_PATH], second[MAX_PATH], output[MAX_PATH], child[MAX_PATH]; DWORD error;
    HANDLE lock, tx; PSECURITY_DESCRIPTOR sd = NULL; SECURITY_ATTRIBUTES sa = {sizeof(sa)};
    STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION owner = {0};
    WCHAR command[] = L"C:\\Windows\\System32\\cmd.exe /c exit 0";
    log = fopen("C:\\VxKexProbe\\NextParity\\setup-cache-result.txt", "wt"); if (!log) ExitProcess(2); setbuf(log, NULL);
    check(GetFileAttributes(TREE) == INVALID_FILE_ATTRIBUTES, "fixture does not preexist");
    if (failures) goto Done;
    check(CreateDirectory(TREE, NULL), "create dedicated fixture");
    error = VistaSetupStagePackage(PACKAGE L"\\Missing", TREE L"\\Cache", output, ARRAYSIZE(output));
    fprintf(log, "MissingPackageStatus=%lu\n", error);
    check((error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND) && !output[0] && GetFileAttributes(TREE L"\\Cache") == INVALID_FILE_ATTRIBUTES,
        "invalid package rolls back new cache and marker");
    error = VistaSetupStagePackage(PACKAGE, TREE L"\\Cache", first, ARRAYSIZE(first));
    fprintf(log, "FirstStageStatus=%lu Path=%ls\n", error, first);
    check(!error && first[0] && packages() == 1, "stage actual complete package into unique cache");
    if (error) goto Cleanup;
    check(security(TREE L"\\Cache") && security(TREE L"\\Cache\\Owner.txt") && securityTree(first), "cache and every staged child have protected administrator/system ACL and administrator owner");
    StringCchPrintf(child, ARRAYSIZE(child), L"%s\\VistaSetup.exe", first);
    check(GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES, "staged package contains native helper");
    StringCchPrintf(child, ARRAYSIZE(child), L"%s\\VistaPty\\winpty-agent.exe", first);
    check(GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES, "staging includes VistaPty payload");
    StringCchPrintf(child, ARRAYSIZE(child), L"%s\\VistaSetup.exe", first);
    lock = CreateFile(child, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "hold earlier staged helper open against replacement");
    error = VistaSetupStagePackage(PACKAGE, TREE L"\\Cache", second, ARRAYSIZE(second));
    if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
    fprintf(log, "SecondStageStatus=%lu Path=%ls\n", error, second);
    check(!error && _wcsicmp(first, second) && packages() == 2 && securityTree(second), "second stage uses separate protected package directory");
    lock = CreateFile(PACKAGE L"\\KexCfg.exe", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "lock package input to force staging failure");
    if (lock != INVALID_HANDLE_VALUE) {
        error = VistaSetupStagePackage(PACKAGE, TREE L"\\Cache", output, ARRAYSIZE(output)); CloseHandle(lock);
        fprintf(log, "LockedPackageStatus=%lu\n", error);
        check(error == ERROR_SHARING_VIOLATION && !output[0] && packages() == 2 && securityTree(first) && securityTree(second),
            "failed stage rolls back and retains both earlier packages");
    }
    lock = CreateFile(PACKAGE L"\\VistaPty\\winpty-agent.exe", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(lock != INVALID_HANDLE_VALUE, "lock noncritical payload to fail during directory copy");
    if (lock != INVALID_HANDLE_VALUE) {
        error = VistaSetupStagePackage(PACKAGE, TREE L"\\Cache", output, ARRAYSIZE(output)); CloseHandle(lock);
        fprintf(log, "PayloadLockStatus=%lu\n", error);
        check(error == ERROR_SHARING_VIOLATION && !output[0] && packages() == 2 && securityTree(first) && securityTree(second),
            "partial package copy rolls back without modifying earlier packages");
    }
    check(CreateDirectory(TREE L"\\Insecure", NULL), "create insecure preexisting cache");
    error = VistaSetupStagePackage(PACKAGE, TREE L"\\Insecure", output, ARRAYSIZE(output));
    check(error == ERROR_ACCESS_DENIED && !output[0] && GetFileAttributes(TREE L"\\Insecure\\Owner.txt") == INVALID_FILE_ATTRIBUTES,
        "refuse insecure preexisting cache without changing its contents");
    check(ConvertStringSecurityDescriptorToSecurityDescriptor(L"O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &sd, NULL), "create owned-cache security descriptor");
    if (sd) {
        sa.lpSecurityDescriptor = sd;
        check(CreateDirectory(TREE L"\\Unmarked", &sa), "create secure but unmarked preexisting cache");
        error = VistaSetupStagePackage(PACKAGE, TREE L"\\Unmarked", output, ARRAYSIZE(output));
        check(error == ERROR_ALREADY_EXISTS && !output[0], "refuse unrelated secure directory without ownership marker");
        LocalFree(sd);
    }
    check(!VistaSetupCollectCache(TREE L"\\Cache", NULL) && packages() == 2,
        "packages without completion records remain untouched");
    check(CreateProcess(NULL, command, NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
        NULL, L"C:\\Windows", &startup, &owner), "create owned dispatcher process to test lifetime-aware cleanup");
    if (owner.hProcess) {
        error = VistaSetupCompleteCache(TREE L"\\Cache", first, owner.hProcess);
        check(!error, "mark first package complete with actual dispatcher identity");
        check(!VistaSetupCompleteCache(TREE L"\\Cache", second, GetCurrentProcess()), "mark second package owned by this still-active process");
        error = VistaSetupStagePackage(first, TREE L"\\NestedCache", output, ARRAYSIZE(output));
        StringCchPrintf(child, ARRAYSIZE(child), L"%s\\Completed.bin", output);
        check(!error && GetFileAttributes(child) == INVALID_FILE_ATTRIBUTES && securityTree(output),
            "restaging a completed cache source never copies its old completion authorization");
        check(!VistaSetupCollectCache(TREE L"\\Cache", NULL) && packages() == 2,
            "completed packages remain while their owning dispatchers are alive");
        check(ResumeThread(owner.hThread) != (DWORD)-1 && WaitForSingleObject(owner.hProcess, 10000) == WAIT_OBJECT_0,
            "owned first dispatcher exits before collection");
        CloseHandle(owner.hThread); CloseHandle(owner.hProcess); owner.hProcess = NULL;
        StringCchPrintf(child, ARRAYSIZE(child), L"%s\\VistaPty\\winpty-agent.exe", first);
        lock = CreateFile(child, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        check(lock != INVALID_HANDLE_VALUE, "lock late payload to force cleanup rollback");
        if (lock != INVALID_HANDLE_VALUE) {
            error = VistaSetupCollectCache(TREE L"\\Cache", NULL);
            check(!error && packages() == 2 && securityTree(first) && securityTree(second),
                "locked completed package retains complete tree after transactional rollback");
            StringCchPrintf(child, ARRAYSIZE(child), L"%s\\KexCfg.exe", first);
            check(GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES, "earlier files remain after failed collection");
            CloseHandle(lock);
        }
        check(!VistaSetupCollectCache(TREE L"\\Cache", first) && packages() == 2,
            "explicit keep path protects the newly selected package");
        check(!VistaSetupCollectCache(TREE L"\\Cache", NULL) && packages() == 1 &&
            GetFileAttributes(first) == INVALID_FILE_ATTRIBUTES && securityTree(second),
            "collect completed exited package while retaining active package and owned root");
        check(GetFileAttributes(TREE L"\\Cache\\Owner.txt") != INVALID_FILE_ATTRIBUTES,
            "cache root ownership marker survives collection");
        check(VistaSetupCompleteCache(TREE L"\\Cache", TREE L"\\Unmarked", GetCurrentProcess()) != 0,
            "completion rejects a package outside the authorized cache");
        check(CreateDirectory(TREE L"\\Cache\\Package-not-a-guid", NULL), "create foreign package-like folder");
        check(!VistaSetupCollectCache(TREE L"\\Cache", NULL) &&
            GetFileAttributes(TREE L"\\Cache\\Package-not-a-guid") != INVALID_FILE_ATTRIBUTES,
            "unknown package-like directory is never collected");
        StringCchCopy(child, ARRAYSIZE(child), TREE L"\\Cache\\Package-{00000000-0000-0000-0000-000000000001}");
        check(CreateDirectory(child, NULL), "create owned malformed-record fixture");
        StringCchCat(child, ARRAYSIZE(child), L"\\Completed.bin");
        lock = CreateFile(child, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        check(lock != INVALID_HANDLE_VALUE, "create truncated completion record");
        if (lock != INVALID_HANDLE_VALUE) {
            DWORD count;
            check(WriteFile(lock, "x", 1, &count, NULL) && count == 1, "write malformed completion record");
            CloseHandle(lock);
        }
        check(!VistaSetupCollectCache(TREE L"\\Cache", NULL) && GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES,
            "malformed completion record never authorizes removal");
    }
Cleanup:
    tx = CreateTransaction(NULL, NULL, 0, 0, 0, 0, NULL);
    if (tx != INVALID_HANDLE_VALUE) {
        error = KxCfgpRemoveSetupDirectory(TREE, tx); check(!error, "remove dedicated fixture transactionally");
        check(error ? RollbackTransaction(tx) : CommitTransaction(tx), "finish fixture cleanup"); CloseHandle(tx);
    } else check(FALSE, "create fixture cleanup transaction");
Done:
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
