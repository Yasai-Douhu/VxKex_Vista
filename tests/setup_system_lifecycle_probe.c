#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <sddl.h>
#include <stdio.h>
#define BASE L"C:\\VxKexProbe\\NextParity"
#define IFEO L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"
#define FIXTURE L"VxKexParityRuntime20261001.exe"
#define DISABLED_FIXTURE L"VxKexParityDisabled20261001.exe"
#define PRODUCT L"Software\\VXsoft\\VxKex"
#define STORE L"Software\\VXsoft\\VxKexVistaPreserved"
#define CUSTOM_LOG BASE L"\\SystemLifecycleCustomLogs"
static const char Guard[] = "VxKex setup lifecycle disposable VM 20261001";
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR text) { fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", text, GetLastError()); fflush(log); if (!ok) ++failures; }
static BOOL disposable(void)
{
    char data[sizeof(Guard)]; DWORD size; HANDLE file; BOOL ok;
    file = CreateFile(BASE L"\\DisposableVM.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, data, sizeof(data), &size, NULL) && size == sizeof(Guard) - 1 && !memcmp(data, Guard, size);
    CloseHandle(file); return ok;
}
static DWORD run(PCWSTR program, PCWSTR parameters, PCWSTR output)
{
    WCHAR command[2048]; STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION process;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE file; DWORD code = 1;
    if (FAILED(StringCchPrintf(command, ARRAYSIZE(command), L"\"%s\" %s", program, parameters))) return ERROR_BAD_PATHNAME;
    file = CreateFile(output, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdOutput = startup.hStdError = file;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (CreateProcess(program, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, L"C:\\Windows", &startup, &process)) {
        // Keep one child alive rather than restarting when observation takes time.
        WaitForSingleObject(process.hProcess, INFINITE); GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    } else code = GetLastError();
    CloseHandle(file); return code;
}
static LONG value(HKEY root, PCWSTR path, REGSAM view, PCWSTR name, DWORD type, PCVOID expected, DWORD length)
{
    HKEY key; BYTE data[1024]; DWORD actual, bytes = sizeof(data); LONG error;
    error = RegOpenKeyEx(root, path, 0, KEY_READ | view, &key);
    if (error) return !expected && error == ERROR_FILE_NOT_FOUND ? 0 : error;
    error = RegQueryValueEx(key, name, NULL, &actual, data, &bytes); RegCloseKey(key);
    if (!expected) return error == ERROR_FILE_NOT_FOUND ? 0 : ERROR_INVALID_DATA;
    if (error) return error;
    return actual == type && bytes == length && !memcmp(data, expected, length) ? 0 : ERROR_INVALID_DATA;
}
static BOOL text(HKEY root, PCWSTR path, REGSAM view, PCWSTR name, PCWSTR expected)
{
    return !value(root, path, view, name, REG_SZ, expected, expected ? (DWORD)((wcslen(expected) + 1) * 2) : 0);
}
static BOOL number(HKEY root, PCWSTR path, REGSAM view, PCWSTR name, DWORD expected)
{
    return !value(root, path, view, name, REG_DWORD, &expected, sizeof(expected));
}
static BOOL exists(HKEY root, PCWSTR path)
{
    HKEY key; LONG error = RegOpenKeyEx(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!error) RegCloseKey(key); return !error;
}
static void fixture(REGSAM view, BOOL enabled)
{
    HKEY key; LONG error; DWORD spoof = enabled ? WinVerSpoofWin10 : WinVerSpoofWin7;
    DWORD flags = enabled ? 0x102 : 2, verifier = 0x80000000;
    PCWSTR debugger = L"\"C:\\VxKex\\VistaRun.exe\" --ifeo";
    error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, enabled ? IFEO L"\\" FIXTURE : IFEO L"\\" DISABLED_FIXTURE,
        0, NULL, 0, KEY_ALL_ACCESS | view, NULL, &key, NULL);
    check(!error, "create actual IFEO fixture"); if (error) return;
    check(!RegSetValueEx(key, L"KEX_WinVerSpoof", 0, REG_DWORD, (PBYTE)&spoof, 4), "set runtime fixture spoof preference");
    check(!RegSetValueEx(key, L"ForeignDiagnosticValue", 0, REG_SZ, (PCBYTE)L"keep-me", sizeof(L"keep-me")), "set unrelated IFEO value");
    check(!RegSetValueEx(key, L"GlobalFlag", 0, REG_DWORD, (PBYTE)&flags, 4), "set enabled or disabled verifier state with unrelated flag");
    if (!enabled) { RegCloseKey(key); return; }
    check(!RegSetValueEx(key, L"VerifierFlags", 0, REG_DWORD, (PBYTE)&verifier, 4), "set custom-only verifier flags");
    check(!RegSetValueEx(key, L"VerifierDlls", 0, REG_SZ, (PCBYTE)L"KexDll.dll", sizeof(L"KexDll.dll")), "set runtime fixture provider");
    check(!RegSetValueEx(key, L"Debugger", 0, REG_SZ, (PCBYTE)debugger, (DWORD)((wcslen(debugger) + 1) * 2)), "set owned launcher");
    check(!RegSetValueEx(key, L"KEX_VistaDebugger", 0, REG_SZ, (PCBYTE)debugger, (DWORD)((wcslen(debugger) + 1) * 2)), "set launcher ownership marker");
    RegCloseKey(key);
}
static void preferences(BOOL create)
{
    HKEY key; DWORD zero = 0; LONG error;
    if (create) {
        check(CreateDirectory(CUSTOM_LOG, NULL) || GetLastError() == ERROR_ALREADY_EXISTS, "create external custom log directory");
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PRODUCT, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &key);
        check(!error, "open real global preferences");
        if (!error) { check(!RegSetValueEx(key, L"LogDir", 0, REG_SZ, (PCBYTE)CUSTOM_LOG, sizeof(CUSTOM_LOG)), "set real custom log directory"); RegCloseKey(key); }
        error = RegCreateKeyEx(HKEY_CURRENT_USER, PRODUCT, 0, NULL, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, NULL, &key, NULL);
        check(!error, "open initiating user's real preferences");
        if (!error) { check(!RegSetValueEx(key, L"EnableLogging", 0, REG_DWORD, (PBYTE)&zero, 4), "set real user logging preference"); RegCloseKey(key); }
    }
    check(text(HKEY_LOCAL_MACHINE, PRODUCT, KEY_WOW64_64KEY, L"LogDir", CUSTOM_LOG) &&
        number(HKEY_CURRENT_USER, PRODUCT, KEY_WOW64_64KEY, L"EnableLogging", 0), "real global and initiating-user preferences retained");
}
static void profileState(BOOL installed)
{
    unsigned view; REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    for (view = 0; view < 2; ++view) {
        check(text(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[view], L"ForeignDiagnosticValue", L"keep-me") &&
            text(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"ForeignDiagnosticValue", L"keep-me"), "unrelated actual IFEO values retained");
        check(number(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[view], L"GlobalFlag", installed ? 0x102 : 2) &&
            number(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"GlobalFlag", 2), "actual enabled and disabled flags retain unrelated bit");
        if (installed) check(number(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"KEX_WinVerSpoof", WinVerSpoofWin7) &&
            text(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"Debugger", NULL) &&
            text(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"VerifierDlls", NULL), "disabled profile preferences persist without enabling its launcher or provider");
        else check(!value(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[view], L"KEX_WinVerSpoof", 0, NULL, 0), "uninstalled disabled profile has no active compatibility values");
    }
}
static void runtime(PCWSTR phase, BOOL enabled)
{
    WCHAR path[MAX_PATH], output[MAX_PATH], report[MAX_PATH], args[MAX_PATH + 32]; DWORD code, bytes;
    unsigned view; char contents[256], expected[64]; HANDLE file;
    for (view = 0; view < 2; ++view) {
        StringCchPrintf(path, ARRAYSIZE(path), BASE L"\\%s\\%s", view ? L"Wow" : L"Native", enabled ? FIXTURE : DISABLED_FIXTURE);
        StringCchPrintf(output, ARRAYSIZE(output), BASE L"\\lifecycle-%s-%s-%s.txt", phase, enabled ? L"enabled" : L"disabled", view ? L"x86" : L"x64");
        StringCchPrintf(report, ARRAYSIZE(report), BASE L"\\lifecycle-%s-%s-%s.report.txt", phase, enabled ? L"enabled" : L"disabled", view ? L"x86" : L"x64");
        StringCchPrintf(args, ARRAYSIZE(args), L"--report \"%s\" %s", report, enabled ? L"" : L"--disabled");
        DeleteFile(report); // Remove only this probe's old diagnostic output.
        code = run(path, args, output); fprintf(log, "Runtime phase=%ls view=%u exit=%08lx\n", phase, view, code);
        check(!code, enabled ? "enabled actual runtime loads KexDll and spoofs both APIs" : "disabled actual runtime stays native without KexDll");
        file = CreateFile(report, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        check(file != INVALID_HANDLE_VALUE, "child writes its own report independently of launcher standard handles");
        if (file != INVALID_HANDLE_VALUE) {
            bytes = 0; check(ReadFile(file, contents, sizeof(contents) - 1, &bytes, NULL), "read actual child report"); CloseHandle(file);
            contents[bytes] = 0;
            sprintf(expected, "ProcessBits=%u KexDllLoaded=%u", view ? 32 : 64, enabled ? 1 : 0);
            check(strstr(contents, expected) && (enabled ? strstr(contents, "GetVersionEx=10.0.19045 RtlGetVersion=10.0.19045 Status=00000000") != NULL :
                (strstr(contents, "GetVersionEx=6.0.") && strstr(contents, "RtlGetVersion=6.0.") && strstr(contents, "Status=00000000"))),
                "actual child report confirms process bits, loaded DLL and both API return values");
        }
    }
}
static void registrations(BOOL installed)
{
    unsigned view; REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    check(text(HKEY_LOCAL_MACHINE, PRODUCT, KEY_WOW64_64KEY, L"KexDir", installed ? L"C:\\VxKex" : NULL), "installation path marker matches lifecycle state");
    check(text(HKEY_LOCAL_MACHINE, L"Software\\Classes\\VxKexVista.Log", KEY_WOW64_64KEY, L"VxKexOwnerPath", installed ? L"C:\\VxKex\\VxlView.exe" : NULL), "log viewer ownership matches lifecycle state");
    check(text(HKEY_LOCAL_MACHINE, L"Software\\Classes\\CLSID\\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}\\InProcServer32",
        KEY_WOW64_64KEY, NULL, installed ? L"C:\\VxKex\\KexShlEx.dll" : NULL), "shell extension server matches lifecycle state");
    for (view = 0; view < 2; ++view) check(text(HKEY_LOCAL_MACHINE, IFEO L"\\{VxKexPropagationVirtualKey}", views[view],
        L"VerifierDlls", installed ? L"KexDll.dll" : NULL), "actual IFEO propagation view matches lifecycle state");
    check((GetFileAttributes(L"C:\\VxKex\\VistaRun.exe") != INVALID_FILE_ATTRIBUTES) == installed, "launcher file matches lifecycle state");
    check((GetFileAttributes(L"C:\\Windows\\System32\\KexDll.dll") != INVALID_FILE_ATTRIBUTES) == installed &&
        (GetFileAttributes(L"C:\\Windows\\SysWOW64\\KexDll.dll") != INVALID_FILE_ATTRIBUTES) == installed, "both real system provider DLLs match lifecycle state");
}
VOID __cdecl mainCRTStartup(VOID)
{
    PWSTR sid = NULL; HANDLE token; PTOKEN_USER user = NULL; DWORD bytes = 0, code; WCHAR args[1024], output[MAX_PATH];
    HKEY key; unsigned i; REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
    PCWSTR modes[] = {L"install", L"uninstall-keep", L"install", L"uninstall-remove"};
    log = fopen("C:\\VxKexProbe\\NextParity\\setup-system-lifecycle.txt", "wt"); if (!log) ExitProcess(2);
    check(disposable(), "explicit disposable VM marker required before any production write");
    if (failures) goto Done;
    check(!KxCfgpElevationRequired(), "requires elevated disposable-VM test process"); if (failures) goto Done;
    // Refuse to adopt an earlier partial fixture or existing saved store.
    for (i = 0; i < 2; ++i) {
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, 0, KEY_READ | views[i], &key);
        check(code == ERROR_FILE_NOT_FOUND, "actual test IFEO fixture does not preexist"); if (!code) RegCloseKey(key);
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, 0, KEY_READ | views[i], &key);
        check(code == ERROR_FILE_NOT_FOUND, "actual disabled fixture does not preexist"); if (!code) RegCloseKey(key);
    }
    check(!exists(HKEY_LOCAL_MACHINE, STORE), "no preexisting preservation store"); if (failures) goto Done;
    check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token), "open initiating token"); if (failures) goto Done;
    GetTokenInformation(token, TokenUser, NULL, 0, &bytes); user = HeapAlloc(GetProcessHeap(), 0, bytes);
    check(user && GetTokenInformation(token, TokenUser, user, bytes, &bytes) && ConvertSidToStringSid(user->User.Sid, &sid), "capture initiating SID");
    CloseHandle(token); if (failures) goto Done;
    for (i = 0; i < ARRAYSIZE(modes); ++i) {
        StringCchPrintf(args, ARRAYSIZE(args), L"--%s %s --user-sid %s", modes[i],
            i == 0 || i == 2 ? L"\"" BASE L"\\RealPackage\"" : L"", sid);
        StringCchPrintf(output, ARRAYSIZE(output), BASE L"\\lifecycle-stage-%u.txt", i);
        code = run(BASE L"\\VistaSetup.exe", args, output);
        fprintf(log, "Lifecycle stage=%u mode=%ls exit=%08lx\n", i, modes[i], code);
        check(!code, "commit actual production setup stage"); if (code) break;
        registrations(i == 0 || i == 2);
        if (i == 0) {
            fixture(views[0], TRUE); fixture(views[1], TRUE);
            fixture(views[0], FALSE); fixture(views[1], FALSE);
            preferences(TRUE); profileState(TRUE); runtime(L"installed", TRUE); runtime(L"installed", FALSE);
        } else if (i == 1) {
            check(exists(HKEY_LOCAL_MACHINE, STORE), "keep uninstall retains committed preservation store");
            check(text(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[0], L"Debugger", NULL) &&
                text(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[1], L"Debugger", NULL), "keep uninstall removes both fixture launchers");
            preferences(FALSE); profileState(FALSE);
        } else if (i == 2) {
            check(!exists(HKEY_LOCAL_MACHINE, STORE), "reinstall consumes committed preservation store");
            check(number(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[0], L"KEX_WinVerSpoof", WinVerSpoofWin10) &&
                number(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[1], L"KEX_WinVerSpoof", WinVerSpoofWin10), "both actual view preferences restored");
            preferences(FALSE); profileState(TRUE); runtime(L"restored", TRUE); runtime(L"restored", FALSE);
        } else {
            check(!exists(HKEY_LOCAL_MACHINE, STORE) && !exists(HKEY_LOCAL_MACHINE, PRODUCT) && !exists(HKEY_CURRENT_USER, PRODUCT), "remove-all removes saved store and real global/user preferences");
            profileState(FALSE);
            check(GetFileAttributes(CUSTOM_LOG) != INVALID_FILE_ATTRIBUTES, "remove-all does not delete external custom logs");
        }
        if (failures) break; // Preserve the failing state for debugger analysis.
    }
    if (!failures) for (i = 0; i < 2; ++i) {
        check(!RegDeleteKeyEx(HKEY_LOCAL_MACHINE, IFEO L"\\" FIXTURE, views[i], 0), "clean this probe's real IFEO fixture key");
        check(!RegDeleteKeyEx(HKEY_LOCAL_MACHINE, IFEO L"\\" DISABLED_FIXTURE, views[i], 0), "clean this probe's disabled fixture key");
    }
Done:
    if (sid) LocalFree(sid); if (user) HeapFree(GetProcessHeap(), 0, user);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
