#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <sddl.h>
#include <stdio.h>
BOOL KexCfgAppendArgument(PWSTR Command, SIZE_T Capacity, PCWSTR Argument);
DWORD KexCfgRunWriter(HWND Owner, PCWSTR Arguments);
#define BASE L"C:\\VxKexProbe\\NextParity"
#define CLASSROOT L"Software\\Classes\\"
#define MENUPATH L"Msi.Package\\shell\\open_vxkex\\command"
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR text) { fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", text, GetLastError()); fflush(log); if (!ok) ++failures; }
static BOOL guard(void)
{
    const char expected[] = "VxKex setup lifecycle disposable VM 20261001";
    char data[sizeof(expected)]; HANDLE file; DWORD count; BOOL ok;
    file = CreateFile(BASE L"\\DisposableVM.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, data, sizeof(data), &count, NULL) && count == sizeof(expected) - 1 && !memcmp(data, expected, count);
    CloseHandle(file); return ok;
}
static BOOL exists(HKEY root, PCWSTR path, REGSAM view)
{
    HKEY key; LONG error = RegOpenKeyEx(root, path, 0, KEY_READ | view, &key);
    if (!error) RegCloseKey(key); return !error;
}
static DWORD run(PCWSTR tool, PCWSTR args)
{
    WCHAR command[2048]; STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION process; DWORD code;
    StringCchPrintf(command, ARRAYSIZE(command), L"\"" BASE L"\\%s\" %s", tool, args);
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, L"C:\\Windows", &startup, &process)) return GetLastError();
    WaitForSingleObject(process.hProcess, INFINITE); GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess); fprintf(log, "Tool=%ls Exit=%lu\n", tool, code); fflush(log); return code;
}
static void logging(BOOLEAN expected, PCWSTR dir)
{
    BOOLEAN enabled; WCHAR path[MAX_PATH];
    check(KxCfgQueryLoggingSettings(&enabled, path, ARRAYSIZE(path)) && enabled == expected && !wcscmp(path, dir), "actual initiating-user logging preferences match expectation");
}
static void msi(BOOL enabled)
{
    unsigned index; KXCFG_PROGRAM_CONFIGURATION config; PCWSTR paths[] = {L"C:\\Windows\\System32\\msiexec.exe", L"C:\\Windows\\SysWOW64\\msiexec.exe"};
    for (index = 0; index < 2; ++index) {
        BOOL got = KxCfgGetConfiguration(paths[index], &config);
        check(enabled ? got && config.Enabled : !got || !config.Enabled, "actual native and WOW64 MSI configurations match expectation");
    }
}
static void bulk(void)
{
    PCWSTR First = BASE L"\\BulkFirst.exe", Second = BASE L"\\BulkSecond.exe";
    WCHAR Self[MAX_PATH]; KXCFG_PROGRAM_CONFIGURATION config;
    BOOL MadeFirst = FALSE, MadeSecond = FALSE;
    check(GetFileAttributes(First) == INVALID_FILE_ATTRIBUTES && GetFileAttributes(Second) == INVALID_FILE_ATTRIBUTES &&
        !KxCfgGetConfiguration(First, &config) && !KxCfgGetConfiguration(Second, &config), "bulk fixtures have no preexisting files or profiles");
    if (failures) return;
    GetModuleFileName(NULL, Self, ARRAYSIZE(Self));
    MadeFirst = CopyFile(Self, First, TRUE); MadeSecond = CopyFile(Self, Second, TRUE);
    check(MadeFirst && MadeSecond, "create two owned bulk image fixtures");
    if (!MadeFirst || !MadeSecond) goto Done;
    check(run(L"KexCfg-global.exe", L"/ADD \"" BASE L"\\BulkFirst.exe\" relative.exe") == ERROR_INVALID_PARAMETER,
        "reject entire bulk request when a later path is invalid");
    check(!KxCfgGetConfiguration(First, &config), "invalid later argument leaves earlier image unconfigured");
    {
        HANDLE Image; IMAGE_DOS_HEADER Dos; DWORD Bytes; WORD Versions[2] = {6, 1}; HKEY Key;
        WCHAR Debugger[128]; NTSTATUS Status;
        Image = CreateFile(Second, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        check(Image != INVALID_HANDLE_VALUE, "open only owned second fixture for subsystem conflict test");
        if (Image == INVALID_HANDLE_VALUE) goto Done;
        check(ReadFile(Image, &Dos, sizeof(Dos), &Bytes, NULL) && Bytes == sizeof(Dos) && Dos.e_magic == IMAGE_DOS_SIGNATURE,
            "read owned fixture PE header");
        if (!failures) {
            SetFilePointer(Image, Dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS32, OptionalHeader) +
                FIELD_OFFSET(IMAGE_OPTIONAL_HEADER32, MajorSubsystemVersion), NULL, FILE_BEGIN);
            check(WriteFile(Image, Versions, sizeof(Versions), &Bytes, NULL) && Bytes == sizeof(Versions),
                "owned second fixture requires subsystem launcher");
        }
        CloseHandle(Image); if (failures) goto Done;
        check(KxCfgpCreateIfeoKeyForProgram(Second, &Key, NULL), "create only owned second image IFEO fixture");
        if (failures) goto Done;
        check(!RegWriteString(Key, NULL, L"Debugger", L"VxKex parity foreign debugger fixture"), "inject foreign debugger into second bulk image"); RegCloseKey(Key);
        check(run(L"KexCfg-global.exe", L"/ADD \"" BASE L"\\BulkFirst.exe\" \"" BASE L"\\BulkSecond.exe\"") == ERROR_ALREADY_EXISTS,
            "later bulk image debugger conflict reports error 183");
        check(!KxCfgGetConfiguration(First, &config), "runtime bulk failure rolls back earlier successful profile creation");
        Status = KxCfgpOpenIfeoKey(Second, &Key); check(NT_SUCCESS(Status), "reopen exact owned debugger fixture after rollback");
        if (NT_SUCCESS(Status)) {
            check(!RegReadString(Key, NULL, L"Debugger", Debugger, ARRAYSIZE(Debugger)) && !wcscmp(Debugger, L"VxKex parity foreign debugger fixture"),
                "bulk rollback preserves foreign debugger"); RegCloseKey(Key);
        }
        check(KxCfgpCreateIfeoKeyForProgram(Second, &Key, NULL), "reopen writable owned debugger fixture for cleanup");
        if (!failures) { check(!RegDeleteValue(Key, L"Debugger"), "remove only injected foreign debugger"); RegCloseKey(Key); }
        if (failures) goto Done;
    }
    check(!run(L"KexCfg-global.exe", L"/ADD \"" BASE L"\\BulkFirst.exe\" \"" BASE L"\\BulkSecond.exe\""), "commit two actual bulk profiles");
    check(KxCfgGetConfiguration(First, &config) && config.Enabled && KxCfgGetConfiguration(Second, &config) && config.Enabled,
        "both bulk profiles enabled");
    check(!run(L"KexCfg-global.exe", L"/CLEAN \"" BASE L"\\BulkFirst.exe\" \"" BASE L"\\BulkSecond.exe\""), "clean request accepts existing images");
    check(KxCfgGetConfiguration(First, &config) && config.Enabled && KxCfgGetConfiguration(Second, &config) && config.Enabled,
        "clean preserves both existing image profiles");
    check(DeleteFile(First), "remove only first owned image to simulate stale configuration");
    check(!run(L"KexCfg-global.exe", L"/CLEAN \"" BASE L"\\BulkFirst.exe\" \"" BASE L"\\BulkSecond.exe\""), "clean mixed existing and stale image request");
    check(!KxCfgGetConfiguration(First, &config) && KxCfgGetConfiguration(Second, &config) && config.Enabled,
        "clean removes missing fixed-drive image and preserves live image");
    check(!run(L"KexCfg-global.exe", L"/DELETE \"" BASE L"\\BulkFirst.exe\" \"" BASE L"\\BulkSecond.exe\""), "delete both profiles including already removed profile");
    check(!KxCfgGetConfiguration(First, &config) && !KxCfgGetConfiguration(Second, &config), "bulk deletion leaves no fixture configurations");
Done:
    if (MadeFirst) DeleteFile(First); if (MadeSecond) DeleteFile(Second);
}
static void writerClient(void)
{
    WCHAR command[1024] = L"test", TinyBuffer[8] = L"test"; int count, index; PWSTR *args;
    PCWSTR values[] = {L"C:\\Directory with spaces\\", L"\u65e5\u672c\u8a9e\\\u672b\u5c3e\\", L"quoted\"value", L""};
    for (index = 0; index < ARRAYSIZE(values); ++index)
        check(KexCfgAppendArgument(command, ARRAYSIZE(command), values[index]), "append client argument with quoting");
    args = CommandLineToArgvW(command, &count);
    check(args && count == ARRAYSIZE(values) + 1, "client command roundtrip argument count");
    if (args && count == ARRAYSIZE(values) + 1)
        for (index = 0; index < ARRAYSIZE(values); ++index)
            check(!wcscmp(args[index + 1], values[index]), "client argument survives spaces Unicode quotes and trailing backslash");
    if (args) LocalFree(args);
    check(!KexCfgAppendArgument(TinyBuffer, ARRAYSIZE(TinyBuffer), L"too long"), "client rejects argument overflow before launch");
    check(KexCfgRunWriter(NULL, L"/GLOBAL") == ERROR_INVALID_PARAMETER, "client obtains real installed writer validation exit code");
}
VOID __cdecl mainCRTStartup(VOID)
{
    HANDLE token = NULL; PTOKEN_USER user = NULL; PWSTR sid = NULL; DWORD bytes, code; WCHAR arguments[1600];
    HKEY key; BOOL installed = FALSE, foreignInjected = FALSE; BOOLEAN extended, initialLogging;
    WCHAR initialLogDir[MAX_PATH];
    log = fopen("C:\\VxKexProbe\\NextParity\\global-settings-cli.txt", "wt"); if (!log) ExitProcess(2);
    check(guard(), "disposable VM marker required before any actual installation or settings write"); if (failures) goto Done;
    check(!KxCfgpElevationRequired(), "operator is elevated");
    check(GetFileAttributes(L"C:\\VxKex") == INVALID_FILE_ATTRIBUTES &&
        !exists(HKEY_CURRENT_USER, L"Software\\VXsoft\\VxKex", KEY_WOW64_64KEY), "fresh installation and no preexisting operator preferences");
    check(!exists(HKEY_LOCAL_MACHINE, CLASSROOT L"exefile\\shell\\open_vxkex", KEY_WOW64_64KEY) &&
        !exists(HKEY_LOCAL_MACHINE, CLASSROOT MENUPATH, KEY_WOW64_64KEY), "no preexisting context menu fixtures");
    check(!exists(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files", KEY_WOW64_64KEY),
        "never adopt a preexisting cleanup handler fixture");
    check(!exists(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msiexec.exe", KEY_WOW64_64KEY) &&
        !exists(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msiexec.exe", KEY_WOW64_32KEY),
        "never adopt preexisting native or WOW64 MSI profiles");
    if (failures) goto Done;
    check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token), "open real operator token"); if (failures) goto Done;
    bytes = 0; GetTokenInformation(token, TokenUser, NULL, 0, &bytes); user = HeapAlloc(GetProcessHeap(), 0, bytes);
    check(user && GetTokenInformation(token, TokenUser, user, bytes, &bytes) && ConvertSidToStringSid(user->User.Sid, &sid), "capture real loaded user SID"); if (failures) goto Done;
    StringCchPrintf(arguments, ARRAYSIZE(arguments), L"--install \"" BASE L"\\RealPackage\" --user-sid %s", sid);
    check(!run(L"VistaSetup.exe", arguments), "install actual package for CLI test"); if (failures) goto Done; installed = TRUE;
    writerClient(); if (failures) goto Cleanup;
    bulk(); if (failures) goto Cleanup;
    check(KxCfgQueryLoggingSettings(&initialLogging, initialLogDir, ARRAYSIZE(initialLogDir)), "capture actual initial logging configuration before dry run");
    fprintf(log, "InitialLogging=%u InitialLogDir=%ls\n", initialLogging, initialLogDir);
    StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/CHECKGLOBAL /USER-SID:%s /LOGGING:0 /LOGDIR:C:\\CliCheckLogs /MSI:1 /CONTEXTMENU:1 /EXTENDED:1", sid);
    check(!run(L"KexCfg-global.exe", arguments), "check-only global CLI succeeds");
    logging(initialLogging, initialLogDir); msi(FALSE);
    check(!KxCfgQueryShellContextMenuEntries(&extended) && !exists(HKEY_CURRENT_USER, L"Software\\VXsoft\\VxKex", KEY_WOW64_64KEY), "check-only rolls back menu and all new user preference keys");
    StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/GLOBAL /USER-SID:%s /LOGGING:0 /LOGDIR:C:\\CliCommittedLogs /MSI:1 /CONTEXTMENU:1 /EXTENDED:1", sid);
    check(!run(L"KexCfg-global.exe", arguments), "commit complete global CLI request");
    logging(FALSE, L"C:\\CliCommittedLogs"); msi(TRUE);
    check(KxCfgQueryShellContextMenuEntries(&extended) && extended, "actual extended context menu committed");
    if (failures) goto Cleanup;
    check(!RegOpenKeyEx(HKEY_LOCAL_MACHINE, CLASSROOT MENUPATH, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key), "open only test-created MSI menu field");
    if (!failures) { check(!RegWriteString(key, NULL, NULL, L"foreign.exe \"%1\""), "inject late menu ownership conflict"); RegCloseKey(key); foreignInjected = TRUE; }
    StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/GLOBAL /USER-SID:%s /LOGGING:1 /LOGDIR:C:\\CliRollbackLogs /MSI:0 /CONTEXTMENU:1 /EXTENDED:0", sid);
    check(run(L"KexCfg-global.exe", arguments) == ERROR_ALREADY_EXISTS, "late menu conflict reports error 183");
    logging(FALSE, L"C:\\CliCommittedLogs"); msi(TRUE);
    check(KxCfgQueryShellContextMenuEntries(&extended) && extended, "late conflict restores prior EXE menu marker");
    check(run(L"KexCfg-global.exe", L"/GLOBAL /USER-SID:S-1-5-18 /LOGGING:1 /LOGDIR:C:\\BadSidLogs /MSI:0 /CONTEXTMENU:0 /EXTENDED:0") == ERROR_INVALID_PARAMETER,
        "reject non-user SID");
Cleanup:
    if (foreignInjected && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, CLASSROOT MENUPATH, 0, KEY_WRITE | KEY_WOW64_64KEY, &key)) {
        check(!RegWriteString(key, NULL, NULL, L"\"C:\\VxKex\\VistaRun.exe\" --with-kex \"C:\\Windows\\System32\\msiexec.exe\" /i \"%1\""), "restore only injected test menu command"); RegCloseKey(key);
    }
    if (installed) {
        StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/GLOBAL /USER-SID:%s /LOGGING:0 /LOGDIR:C:\\CliCommittedLogs /MSI:0 /CONTEXTMENU:0 /EXTENDED:0", sid);
        check(!run(L"KexCfg-global.exe", arguments), "disable test-created MSI and context integration before teardown");
        StringCchPrintf(arguments, ARRAYSIZE(arguments), L"--uninstall-remove --user-sid %s", sid);
        check(!run(L"VistaSetup.exe", arguments), "remove actual diagnostic installation");
        check(!RegDeleteTree(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files"), "remove only cleanup handler whose absence was verified before test");
    }
Done:
    if (sid) LocalFree(sid); if (user) HeapFree(GetProcessHeap(), 0, user); if (token) CloseHandle(token);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
