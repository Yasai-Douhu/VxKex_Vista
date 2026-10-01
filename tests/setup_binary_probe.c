#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <ktmw32.h>
#include <stdio.h>
#ifdef _WIN64
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupBinary-x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-binary-x64.txt"
#else
#define ROOT L"C:\\VxKexProbe\\NextParity\\SetupBinary-x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\setup-binary-x86.txt"
#endif
#define INPUT L"C:\\VxKexProbe\\NextParity\\RealPackage"
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR name)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", name, GetLastError());
    fflush(log); if (!ok) ++failures;
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
static BOOL sameFile(PCWSTR first, PCWSTR second)
{
    HANDLE a = CreateFile(first, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE b = CreateFile(second, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE ba[4096], bb[4096]; DWORD na, nb; BOOL same = FALSE;
    if (a == INVALID_HANDLE_VALUE || b == INVALID_HANDLE_VALUE) goto Done;
    for (;;) {
        if (!ReadFile(a, ba, sizeof(ba), &na, NULL) || !ReadFile(b, bb, sizeof(bb), &nb, NULL)) break;
        if (na != nb || memcmp(ba, bb, na)) break;
        if (!na) { same = TRUE; break; }
    }
Done:
    if (a != INVALID_HANDLE_VALUE) CloseHandle(a);
    if (b != INVALID_HANDLE_VALUE) CloseHandle(b); return same;
}
static BOOL mutate(HANDLE tx, unsigned mode)
{
    HANDLE file = CreateFileTransacted(ROOT L"\\Bad.dll", GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL, tx, NULL, NULL);
    IMAGE_DOS_HEADER dos; IMAGE_FILE_HEADER header; LARGE_INTEGER position; DWORD count, value = 0;
    BOOL ok = FALSE;
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!ReadFile(file, &dos, sizeof(dos), &count, NULL) || count != sizeof(dos)) goto Done;
    position.QuadPart = dos.e_lfanew + sizeof(DWORD);
    if (!SetFilePointerEx(file, position, NULL, FILE_BEGIN) ||
        !ReadFile(file, &header, sizeof(header), &count, NULL) || count != sizeof(header)) goto Done;
    switch (mode) {
    case 0: position.QuadPart = 2; ok = SetFilePointerEx(file, position, NULL, FILE_BEGIN) && SetEndOfFile(file); goto Done;
    case 1: position.QuadPart = FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew); value = 0x7fffffff; break;
    case 2: position.QuadPart = dos.e_lfanew; value = 0; break;
    case 3: position.QuadPart = dos.e_lfanew + sizeof(DWORD) + sizeof(header); value = IMAGE_NT_OPTIONAL_HDR32_MAGIC; break;
    case 4: position.QuadPart = dos.e_lfanew + sizeof(DWORD) + sizeof(header) + header.SizeOfOptionalHeader + FIELD_OFFSET(IMAGE_SECTION_HEADER, SizeOfRawData); value = 0xffffffff; break;
    case 5: position.QuadPart = FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew); value = 0xffffffff; break;
    default: goto Done;
    }
    ok = SetFilePointerEx(file, position, NULL, FILE_BEGIN) && WriteFile(file, &value, sizeof(value), &count, NULL) && count == sizeof(value);
Done:
    CloseHandle(file); return ok;
}
void __cdecl mainCRTStartup(void)
{
    HANDLE tx; LONG error; unsigned mode;
    log = fopen(LOGFILE, "w"); if (!log) ExitProcess(2);
    if (GetFileAttributes(ROOT) != INVALID_FILE_ATTRIBUTES) { check(FALSE, "refuse existing fixture"); fclose(log); ExitProcess(3); }
    check(CreateDirectory(ROOT, NULL), "create dedicated root");
    check(CreateDirectory(ROOT L"\\Native", NULL), "create native fixture");
    check(CreateDirectory(ROOT L"\\Wow", NULL), "create WOW fixture");
    tx = begin();
    error = KxCfgpValidateSetupPackage(INPUT, tx);
    fprintf(log, "RealPackageStatus=%ld\n", error); check(!error, "validate actual Installer binaries");
    check(!KxCfgpValidateSetupBinary(INPUT L"\\Kex32\\KexDll.dll", IMAGE_FILE_MACHINE_I386, TRUE, tx), "valid real x86 DLL");
    check(KxCfgpValidateSetupBinary(INPUT L"\\Kex32\\KexDll.dll", IMAGE_FILE_MACHINE_AMD64, TRUE, tx) == ERROR_BAD_EXE_FORMAT, "reject x86 DLL for native slot");
    check(KxCfgpValidateSetupBinary(INPUT L"\\KexDll.dll", IMAGE_FILE_MACHINE_I386, TRUE, tx) == ERROR_BAD_EXE_FORMAT, "reject x64 DLL for WOW slot");
    check(KxCfgpValidateSetupBinary(INPUT L"\\VistaRun.exe", IMAGE_FILE_MACHINE_AMD64, TRUE, tx) == ERROR_BAD_EXE_FORMAT, "reject EXE for DLL slot");
    check(KxCfgpValidateSetupBinary(INPUT L"\\KexDll.dll", IMAGE_FILE_MACHINE_AMD64, FALSE, tx) == ERROR_BAD_EXE_FORMAT, "reject DLL for EXE slot");
    finish(tx, FALSE);
    if (failures) { fclose(log); ExitProcess(4); }
    for (mode = 0; mode < 6; ++mode) {
        tx = begin(); check(!KxCfgpCopySetupFile(INPUT L"\\KexDll.dll", ROOT L"\\Bad.dll", tx), "copy corruption fixture inside transaction");
        check(mutate(tx, mode), "mutate real DLL fixture");
        error = KxCfgpValidateSetupBinary(ROOT L"\\Bad.dll", IMAGE_FILE_MACHINE_AMD64, TRUE, tx);
        fprintf(log, "CorruptionMode=%u Status=%ld\n", mode, error); check(error == ERROR_BAD_EXE_FORMAT, "reject corrupt PE layout"); finish(tx, FALSE);
        check(GetFileAttributes(ROOT L"\\Bad.dll") == INVALID_FILE_ATTRIBUTES, "corruption fixture rolled back");
    }
    tx = begin();
    error = KxCfgpDeploySetupPackage(INPUT, ROOT L"\\Installed", ROOT L"\\Native", ROOT L"\\Wow", tx);
    check(!error, "stage actual package deployment"); finish(tx, !error);
    check(sameFile(INPUT L"\\KexDll.dll", ROOT L"\\Native\\KexDll.dll"), "native real DLL identical after deployment");
    check(sameFile(INPUT L"\\Kex32\\KexDll.dll", ROOT L"\\Wow\\KexDll.dll"), "WOW real DLL identical after deployment");
    check(sameFile(INPUT L"\\VistaPty\\winpty.dll", ROOT L"\\Installed\\VistaPty\\winpty.dll"), "bundled WinPTY identical after deployment");
    tx = begin();
    check(!KxCfgpCopySetupFile(INPUT L"\\Kex32\\KexDll.dll", ROOT L"\\Installed\\KexDll.dll", tx), "stage wrong architecture in package copy");
    check(KxCfgpValidateSetupPackage(ROOT L"\\Installed", tx) == ERROR_BAD_EXE_FORMAT, "reject mixed-architecture package"); finish(tx, FALSE);
    check(sameFile(INPUT L"\\KexDll.dll", ROOT L"\\Installed\\KexDll.dll"), "mixed package rollback restores DLL");
    tx = begin();
    check(!KxCfgpCopySetupFile(INPUT L"\\Kex32\\KexDll.dll", ROOT L"\\Installed\\KexDll.dll", tx), "prepare invalid package for production entry point");
    check(KxCfgpDeploySetupPackage(ROOT L"\\Installed", ROOT L"\\InvalidDestination", ROOT L"\\Native", ROOT L"\\Wow", tx) == ERROR_BAD_EXE_FORMAT,
        "production entry point rejects invalid package before staging destinations");
    finish(tx, FALSE);
    check(GetFileAttributes(ROOT L"\\InvalidDestination") == INVALID_FILE_ATTRIBUTES &&
        sameFile(INPUT L"\\KexDll.dll", ROOT L"\\Native\\KexDll.dll"), "invalid production deployment leaves destinations unchanged");
    tx = begin(); error = KxCfgpRemoveSetupFiles(ROOT L"\\Installed", ROOT L"\\Native", ROOT L"\\Wow", tx);
    check(!error, "stage real package removal"); finish(tx, !error);
    check(GetFileAttributes(ROOT L"\\Installed") == INVALID_FILE_ATTRIBUTES, "actual package removed from fixture");
    tx = begin(); error = KxCfgpRemoveSetupDirectory(ROOT, tx); check(!error, "clean dedicated root"); finish(tx, !error);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
