#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <stdio.h>
#include <wchar.h>
#define BASE L"C:\\VxKexProbe\\NextParity"
static FILE *log;
static unsigned failures;
static void check(BOOL ok, const char *what) { fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", what, GetLastError()); fflush(log); if (!ok) ++failures; }
static BOOL absent(HKEY root, PCWSTR path) {
    HKEY key; LONG result = RegOpenKeyExW(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!result) RegCloseKey(key); return result == ERROR_FILE_NOT_FOUND;
}
static BOOL same(PCWSTR first, PCWSTR second) {
    HANDLE a, b; BYTE x[4096], y[4096]; DWORD nx, ny; BOOL ok = FALSE;
    a = CreateFileW(first, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    b = CreateFileW(second, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (a == INVALID_HANDLE_VALUE || b == INVALID_HANDLE_VALUE) goto Done;
    for (;;) {
        if (!ReadFile(a, x, sizeof(x), &nx, NULL) || !ReadFile(b, y, sizeof(y), &ny, NULL) || nx != ny || memcmp(x, y, nx)) break;
        if (!nx) { ok = TRUE; break; }
    }
Done: if (a != INVALID_HANDLE_VALUE) CloseHandle(a); if (b != INVALID_HANDLE_VALUE) CloseHandle(b); return ok;
}
static DWORD run(PCWSTR file, PCWSTR args) {
    WCHAR command[2048]; STARTUPINFOW startup = {sizeof(startup)}; PROCESS_INFORMATION process; DWORD code;
    _snwprintf(command, 2048, L"\"" BASE L"\\%s\" %s", file, args); command[2047] = 0;
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, L"C:\\Windows", &startup, &process)) return GetLastError();
    WaitForSingleObject(process.hProcess, INFINITE); GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    fprintf(log, "Tool=%ls Exit=0x%08lx\n", file, code); fflush(log); return code;
}
static void registration(BOOL enabled, unsigned arch, PCWSTR phase) {
    WCHAR file[80], report[MAX_PATH], saved[MAX_PATH];
    _snwprintf(file, 80, enabled ? L"TlsRegistered-%s.exe" : L"kxschanl_registration_probe_%s.exe", arch ? L"x86" : L"x64");
    check(!run(file, enabled ? L"" : L"--native"), "registration probe requires actual mode-specific modules and credentials");
    _snwprintf(report, MAX_PATH, BASE L"\\registration-%s.txt", arch ? L"x86" : L"x64");
    _snwprintf(saved, MAX_PATH, BASE L"\\tls-%s-%s.txt", phase, arch ? L"x86" : L"x64");
    check(CopyFileW(report, saved, FALSE), "preserve each independent registration report");
}
int main(int argc, char **argv) {
    const char marker[] = "VxKex setup lifecycle disposable VM 20261001";
    char data[sizeof(marker)]; HANDLE f, token = NULL; DWORD bytes, count, beforeSize = 4096, afterSize = 4096, beforeType, afterType;
    BYTE before[4096], after[4096]; HKEY providers; PTOKEN_USER user = NULL; PWSTR sid = NULL; WCHAR args[2048], source[MAX_PATH], dest[MAX_PATH];
    BOOL installed = FALSE, made[2] = {FALSE,FALSE}; unsigned arch, index;
    PCWSTR names[] = {L"KexDll.dll", L"KxAdvapi.dll", L"KxCryp.dll", L"KxSChanl.dll"};
    BOOL integrated = argc == 2 && !strcmp(argv[1], "--integrated");
    PCWSTR package = integrated ? BASE L"\\HighPackage" : BASE L"\\TlsPackage";
    PCWSTR setupTool = integrated ? L"HighPackage\\VistaSetup.exe" : L"VistaSetup.exe";
    PCWSTR writerTool = integrated ? L"HighPackage\\KexCfg.exe" : L"KexCfg-global.exe";
    if (argc != 1 && !integrated) return 87;
    log = fopen(integrated ? "C:\\VxKexProbe\\NextParity\\high-package-deployment.txt" : "C:\\VxKexProbe\\NextParity\\tls-package-deployment.txt", "w"); if (!log) return 2;
    f = CreateFileW(BASE L"\\DisposableVM.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    check(f != INVALID_HANDLE_VALUE && ReadFile(f, data, sizeof(data), &count, NULL) && count == sizeof(marker)-1 && !memcmp(data, marker, count), "exact disposable VM marker");
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    check(IsUserAnAdmin(), "elevated disposable VM operator");
    check(GetFileAttributesW(L"C:\\VxKex") == INVALID_FILE_ATTRIBUTES && absent(HKEY_CURRENT_USER, L"Software\\VXsoft\\VxKex") && absent(HKEY_LOCAL_MACHINE, L"Software\\VXsoft\\VxKex"), "fresh installation and preferences required");
    check(absent(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files"), "cleanup handler absent before test");
    for (arch=0; arch<2; ++arch) {
        _snwprintf(source, MAX_PATH, BASE L"\\TlsRegistered-%s.exe", arch ? L"x86" : L"x64");
        _snwprintf(dest, MAX_PATH, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\TlsRegistered-%s.exe", arch ? L"x86" : L"x64");
        check(GetFileAttributesW(source) == INVALID_FILE_ATTRIBUTES && absent(HKEY_LOCAL_MACHINE, dest), "never adopt existing image fixtures");
    }
    if (failures) goto Done;
    check(!RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecurityProviders", 0, KEY_READ, &providers), "open global provider registry read-only");
    if (failures) goto Done;
    check(!RegQueryValueExW(providers, L"SecurityProviders", NULL, &beforeType, before, &beforeSize), "capture original provider bytes and type");
    check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token), "capture actual operator token");
    if (failures) goto CloseProviders;
    GetTokenInformation(token, TokenUser, NULL, 0, &bytes); user = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, bytes);
    check(user && GetTokenInformation(token, TokenUser, user, bytes, &bytes) && ConvertSidToStringSidW(user->User.Sid, &sid), "capture loaded initiating user SID");
    if (failures) goto CloseProviders;
    _snwprintf(args, 2048, L"--install \"%s\" --user-sid %s", package, sid);
    check(!run(setupTool, args), "install staged candidate using real deployment helper");
    if (failures) goto CloseProviders; installed = TRUE;
    for (arch=0; arch<2; ++arch) for (index=0; index<4; ++index) {
        _snwprintf(source, MAX_PATH, L"%s\\%s%s", package, arch ? L"Kex32\\" : L"", names[index]);
        _snwprintf(dest, MAX_PATH, L"C:\\Windows\\%s\\%s", arch ? L"SysWOW64" : L"System32", names[index]);
        check(same(source, dest), "physical system DLL byte-identical to staged source build");
    }
    _snwprintf(source, MAX_PATH, L"%s\\Certificates\\ROOT.sst", package);
    check(same(source, L"C:\\VxKex\\Certificates\\ROOT.sst"), "deployed root bundle byte-identical");
    if (integrated) {
        PCWSTR frontends[] = {L"KexCfg.exe", L"VxlView.exe", L"VistaSetup.exe", L"Kex32\\KexCfg.exe", L"Kex32\\VxlView.exe"};
        for (index=0; index<5; ++index) {
            _snwprintf(source, MAX_PATH, L"%s\\%s", package, frontends[index]);
            _snwprintf(dest, MAX_PATH, L"C:\\VxKex\\%s", frontends[index]);
            check(same(source,dest), "installed settings/viewer/setup frontends match integrated source build");
        }
    }
    for (arch=0; arch<2; ++arch) {
        registration(FALSE, arch, L"native-before");
        _snwprintf(source, MAX_PATH, BASE L"\\kxschanl_registration_probe_%s.exe", arch ? L"x86" : L"x64");
        _snwprintf(dest, MAX_PATH, BASE L"\\TlsRegistered-%s.exe", arch ? L"x86" : L"x64");
        made[arch] = CopyFileW(source, dest, TRUE); check(made[arch], "create unique registered image");
        if (!made[arch]) continue;
        _snwprintf(args, 2048, L"/ADD \"%s\"", dest); check(!run(writerTool, args), "enable exact diagnostic image");
        registration(TRUE, arch, L"registered");
        registration(FALSE, arch, L"native-after");
    }
    check(!RegQueryValueExW(providers, L"SecurityProviders", NULL, &afterType, after, &afterSize) && beforeType == afterType && beforeSize == afterSize && !memcmp(before, after, beforeSize), "global SSP registry unchanged after applied and native processes");
    if (installed) {
        for (arch=0; arch<2; ++arch) if (made[arch]) {
            _snwprintf(dest, MAX_PATH, BASE L"\\TlsRegistered-%s.exe", arch ? L"x86" : L"x64");
            _snwprintf(args, 2048, L"/DELETE \"%s\"", dest); check(!run(writerTool, args), "remove only owned diagnostic profile");
            check(DeleteFileW(dest), "remove only owned diagnostic executable");
        }
        _snwprintf(args, 2048, L"--uninstall-remove --user-sid %s", sid); check(!run(setupTool, args), "uninstall diagnostic deployment");
        {
            LONG error = RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files");
            check((error == ERROR_SUCCESS || error == ERROR_FILE_NOT_FOUND) && absent(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files"), "test-created cleanup handler absent after teardown");
        }
        check(GetFileAttributesW(L"C:\\VxKex") == INVALID_FILE_ATTRIBUTES && absent(HKEY_CURRENT_USER, L"Software\\VXsoft\\VxKex") && absent(HKEY_LOCAL_MACHINE, L"Software\\VXsoft\\VxKex"), "installation and diagnostic preferences removed");
        check(!RegQueryValueExW(providers, L"SecurityProviders", NULL, &afterType, after, &afterSize) && beforeType == afterType && beforeSize == afterSize && !memcmp(before, after, beforeSize), "global SSP registry unchanged after teardown");
    }
CloseProviders: RegCloseKey(providers);
Done: if (sid) LocalFree(sid); if (user) HeapFree(GetProcessHeap(), 0, user); if (token) CloseHandle(token);
    fprintf(log, "Failures=%u\n", failures); fclose(log); return failures ? 1 : 0;
}
