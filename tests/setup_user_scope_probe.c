#include "../VistaSetup/buildcfg.h"
#include <KxCfgHlp.h>
#include <lm.h>
#include <userenv.h>
#include <sddl.h>
#include <stdio.h>
#include <commctrl.h>
#define BASE L"C:\\VxKexProbe\\NextParity"
#define USERNAME L"VxKexScope261001"
#define EXPECTED_PROFILE L"C:\\Users\\VxKexScope261001"
#define PRODUCT L"Software\\VXsoft\\VxKex"
static FILE *log; static unsigned failures;
static void check(BOOL ok, PCSTR message)
{
    fprintf(log, "%s %s error=%lu\n", ok ? "PASS" : "FAIL", message, GetLastError()); fflush(log);
    if (!ok) ++failures;
}
static BOOL guard(void)
{
    const char expected[] = "VxKex setup lifecycle disposable VM 20261001";
    char data[sizeof(expected)]; DWORD size; BOOL ok; HANDLE file;
    file = CreateFile(BASE L"\\DisposableVM.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, data, sizeof(data), &size, NULL) && size == sizeof(expected) - 1 && !memcmp(data, expected, size);
    CloseHandle(file); return ok;
}
static BOOL exists(HKEY root)
{
    HKEY key; LONG error = RegOpenKeyEx(root, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!error) RegCloseKey(key); return !error;
}
static void preference(HKEY root, DWORD expected, BOOL write)
{
    HKEY key; DWORD value, type, bytes = sizeof(value); LONG error;
    if (write) error = RegCreateKeyEx(root, PRODUCT, 0, NULL, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &key, NULL);
    else error = RegOpenKeyEx(root, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    check(!error, "open explicit user preference hive"); if (error) return;
    if (write) check(!RegSetValueEx(key, L"EnableLogging", 0, REG_DWORD, (PBYTE)&expected, 4), "write distinct user preference");
    error = RegQueryValueEx(key, L"EnableLogging", NULL, &type, (PBYTE)&value, &bytes);
    check(!error && type == REG_DWORD && bytes == 4 && value == expected, "explicit user preference has expected value");
    RegCloseKey(key);
}
static BOOL privilege(PCWSTR name)
{
    HANDLE token; TOKEN_PRIVILEGES p; BOOL ok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token)) return FALSE;
    p.PrivilegeCount = 1; p.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ok = LookupPrivilegeValue(NULL, name, &p.Privileges[0].Luid) && AdjustTokenPrivileges(token, FALSE, &p, 0, NULL, NULL) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(token); return ok;
}
static DWORD setup(BOOL install, PCWSTR sid)
{
    WCHAR command[1024]; STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION process;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE}; HANDLE file; DWORD code = 1;
    StringCchPrintf(command, ARRAYSIZE(command), L"\"" BASE L"\\VistaSetup.exe\" --%s %s --user-sid %s",
        install ? L"install" : L"uninstall-remove", install ? L"\"" BASE L"\\RealPackage\"" : L"", sid);
    file = CreateFile(install ? BASE L"\\user-scope-install.txt" : BASE L"\\user-scope-remove.txt", GENERIC_WRITE,
        FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdOutput = startup.hStdError = file; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, L"C:\\Windows", &startup, &process)) {
        WaitForSingleObject(process.hProcess, INFINITE); GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    } else code = GetLastError();
    CloseHandle(file); fprintf(log, "Setup install=%u exit=%lu\n", install, code); return code;
}
static DWORD globalSettings(PCWSTR sid)
{
    WCHAR command[1024]; STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION process; DWORD code;
    StringCchPrintf(command, ARRAYSIZE(command), L"\"" BASE L"\\KexCfg-global.exe\" /GLOBAL /USER-SID:%s /LOGGING:0 /LOGDIR:C:\\SelectedStandardUserLogs /MSI:0 /CONTEXTMENU:0 /EXTENDED:0", sid);
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, L"C:\\Windows", &startup, &process)) return GetLastError();
    WaitForSingleObject(process.hProcess, INFINITE); GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return code;
}
static DWORD GuiPid; static HWND GuiDialog;
static BOOL CALLBACK findStandardGui(HWND Window, LPARAM Unused)
{
    DWORD Pid; WCHAR Class[32];
    GetWindowThreadProcessId(Window, &Pid); GetClassName(Window, Class, ARRAYSIZE(Class));
    if (Pid == GuiPid && !wcscmp(Class, L"#32770") && GetDlgItem(Window, 120)) {
        GuiDialog = Window; return FALSE;
    }
    return TRUE;
}
static void standardGui(HANDLE Primary, HDESK Desktop, STARTUPINFO *Startup, PCWSTR Image)
{
    WCHAR Command[1024];
    PROCESS_INFORMATION Process; DWORD Code, Index; DWORD_PTR Response;
    const int Controls[] = {101,102,103,110,111,112,113,120,121,122,123,124,190,191,192};
    GuiDialog = NULL;
    StringCchPrintf(Command, ARRAYSIZE(Command), L"\"%s\"", Image);
    fwprintf(log, L"StandardGuiImage=%s\n", Image);
    if (!CreateProcessWithTokenW(Primary, 0, Image, Command,
        0, NULL, L"C:\\Windows", Startup, &Process)) {
        check(FALSE, "launch actual GUI as standard user without UAC"); return;
    }
    check(TRUE, "launch actual GUI as standard user without UAC"); GuiPid = Process.dwProcessId;
    WaitForInputIdle(Process.hProcess, 10000);
    for (Index = 0; Index < 100 && !GuiDialog; ++Index) {
        EnumDesktopWindows(Desktop, findStandardGui, 0);
        if (WaitForSingleObject(Process.hProcess, 100) == WAIT_OBJECT_0) break;
    }
    check(!!GuiDialog, "standard GUI creates actual configuration dialog");
    if (GuiDialog) {
        for (Index = 0; Index < ARRAYSIZE(Controls); ++Index)
            check(!!GetDlgItem(GuiDialog, Controls[Index]), "standard GUI contains required NEXT control");
        check(IsDlgButtonChecked(GuiDialog, 101) == BST_CHECKED,
            "GUI reads standard-user logging preference rather than administrator preference");
        check(!IsWindowEnabled(GetDlgItem(GuiDialog, 111)) && !IsWindowEnabled(GetDlgItem(GuiDialog, 190)),
            "unsupported BHO and unchanged Apply are disabled");
        check(SendMessageTimeout(GuiDialog, WM_COMMAND, 192, 0, SMTO_ABORTIFHUNG, 5000, &Response) != 0,
            "command-only Cancel closes unmodified standard GUI");
    }
    if (WaitForSingleObject(Process.hProcess, 5000) != WAIT_OBJECT_0) {
        check(FALSE, "standard GUI closes without orphaning process");
        TerminateProcess(Process.hProcess, 99); WaitForSingleObject(Process.hProcess, 5000);
    }
    check(GetExitCodeProcess(Process.hProcess, &Code) && Code == 0, "standard GUI exits successfully without saving");
    CloseHandle(Process.hThread); CloseHandle(Process.hProcess);
}
static HWND FailureDialog;
static BOOL CALLBACK findOwnedError(HWND Window, LPARAM Unused)
{
    DWORD Pid; WCHAR Class[32];
    GetWindowThreadProcessId(Window, &Pid); GetClassName(Window, Class, ARRAYSIZE(Class));
    if (Pid == GuiPid && Window != GuiDialog && GetWindow(Window, GW_OWNER) == GuiDialog &&
        !wcscmp(Class, L"#32770")) { FailureDialog = Window; return FALSE; }
    return TRUE;
}
static void failAndRetryGui(HDESK Desktop, HANDLE Process, PCWSTR CommittedDirectory)
{
    PCWSTR Conflict = L"Software\\Classes\\Msi.Package\\shell\\open_vxkex";
    PCWSTR ExeMenu = L"Software\\Classes\\exefile\\shell\\open_vxkex";
    PCWSTR Pending = L"%USERPROFILE%\\Retry GUI Logs\\";
    HKEY Key; DWORD Disposition, Index; LONG Error; DWORD_PTR Response;
    WCHAR Directory[MAX_PATH], Expected[MAX_PATH], Label[128]; BOOLEAN Enabled; BOOL Owned = FALSE;
    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, Conflict, 0, KEY_READ | KEY_WOW64_64KEY, &Key);
    check(Error == ERROR_FILE_NOT_FOUND, "never adopt a preexisting MSI menu conflict fixture");
    if (!Error) RegCloseKey(Key); if (failures) return;
    Error = RegCreateKeyEx(HKEY_LOCAL_MACHINE, Conflict, 0, NULL, 0,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &Key, &Disposition);
    check(!Error && Disposition == REG_CREATED_NEW_KEY, "create only owned late MSI conflict key"); if (Error) return;
    Owned = Disposition == REG_CREATED_NEW_KEY;
    check(!RegWriteString(Key, NULL, NULL, L"VxKex parity foreign menu fixture"), "inject foreign MSI menu label"); RegCloseKey(Key);
    if (failures) goto Done;
    SendMessage(GetDlgItem(GuiDialog, 102), EM_SETSEL, 0, -1);
    check(SendMessageTimeout(GetDlgItem(GuiDialog, 102), EM_REPLACESEL, TRUE, (LPARAM)Pending,
        SMTO_ABORTIFHUNG, 5000, &Response) != 0, "edit pending retry directory in actual buffer");
    CheckDlgButton(GuiDialog, 101, BST_UNCHECKED); SendMessage(GuiDialog, WM_COMMAND, 101, 0);
    CheckDlgButton(GuiDialog, 112, BST_CHECKED); SendMessage(GuiDialog, WM_COMMAND, 112, 0);
    FailureDialog = NULL; PostMessage(GuiDialog, WM_COMMAND, 190, 0);
    for (Index = 0; Index < 150 && !FailureDialog; ++Index) {
        EnumDesktopWindows(Desktop, findOwnedError, 0);
        if (WaitForSingleObject(Process, 100) == WAIT_OBJECT_0) break;
    }
    check(!!FailureDialog, "actual GUI reports late writer failure in its owned error dialog");
    check(KxCfgQueryLoggingSettings(&Enabled, Directory, ARRAYSIZE(Directory)) && Enabled &&
        !wcscmp(Directory, CommittedDirectory), "failed GUI save rolls back pending logging and directory");
    Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ExeMenu, 0, KEY_READ | KEY_WOW64_64KEY, &Key);
    check(Error == ERROR_FILE_NOT_FOUND, "late MSI failure rolls back earlier EXE menu creation"); if (!Error) RegCloseKey(Key);
    if (FailureDialog) check(SendMessageTimeout(FailureDialog, TDM_CLICK_BUTTON, IDOK, 0,
        SMTO_ABORTIFHUNG, 5000, &Response) != 0, "dismiss only the diagnostic GUI's owned error dialog");
    for (Index = 0; Index < 50 && !IsWindowEnabled(GuiDialog); ++Index) Sleep(100);
    check(IsWindowEnabled(GuiDialog) && IsWindowEnabled(GetDlgItem(GuiDialog, 190)), "failure retains pending Apply and restores parent window");
Done:
    if (Owned) {
        Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, Conflict, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Key);
        check(!Error, "open owned foreign fixture for exact cleanup");
        if (!Error) {
            check(!RegReadString(Key, NULL, NULL, Label, ARRAYSIZE(Label)) && !wcscmp(Label, L"VxKex parity foreign menu fixture"),
                "writer preserves foreign fixture label");
            if (!failures) check(!RegDeleteValue(Key, NULL), "remove only injected foreign label"); RegCloseKey(Key);
            if (!failures) check(!RegDeleteKeyEx(HKEY_LOCAL_MACHINE, Conflict, KEY_WOW64_64KEY, 0), "remove empty owned conflict key");
        }
    }
    if (failures) return;
    check(PostMessage(GuiDialog, WM_COMMAND, 190, 0), "retry Apply without reentering pending fields");
    for (Index = 0; Index < 150 && IsWindowEnabled(GetDlgItem(GuiDialog, 190)); ++Index)
        if (WaitForSingleObject(Process, 100) == WAIT_OBJECT_0) break;
    ExpandEnvironmentStrings(Pending, Expected, ARRAYSIZE(Expected));
    check(KxCfgQueryLoggingSettings(&Enabled, Directory, ARRAYSIZE(Directory)) && !Enabled && !wcscmp(Directory, Expected),
        "retry saves retained pending checkbox and directory after conflict removal");
    check(!IsWindowEnabled(GetDlgItem(GuiDialog, 190)) && IsWindowEnabled(GuiDialog), "successful retry clears Apply and restores parent window");
    check(KxCfgConfigureShellContextMenuEntries(FALSE, FALSE, NULL), "remove only own context menus created by successful retry");
}
static void saveGui(HDESK Desktop, STARTUPINFO *Startup, PCWSTR Image)
{
    WCHAR Command[1024], Expected[MAX_PATH], Actual[MAX_PATH];
    PCWSTR Input = L"%USERPROFILE%\\VxKex GUI \u4fdd\u5b58\\";
    PROCESS_INFORMATION Process; DWORD Index, Code; DWORD_PTR Response;
    BOOLEAN Enabled = TRUE; HKEY Key;
    GuiDialog = NULL; StringCchPrintf(Command, ARRAYSIZE(Command), L"\"%s\"", Image);
    fwprintf(log, L"SaveGuiImage=%s\n", Image);
    if (!CreateProcess(Image, Command, NULL, NULL, FALSE, 0, NULL, L"C:\\Windows", Startup, &Process)) {
        check(FALSE, "launch actual GUI with elevated operator token for save test"); return;
    }
    GuiPid = Process.dwProcessId; WaitForInputIdle(Process.hProcess, 10000);
    for (Index = 0; Index < 100 && !GuiDialog; ++Index) {
        EnumDesktopWindows(Desktop, findStandardGui, 0);
        if (WaitForSingleObject(Process.hProcess, 100) == WAIT_OBJECT_0) break;
    }
    check(!!GuiDialog, "actual save-test GUI creates dialog on isolated desktop");
    if (GuiDialog) {
        CheckDlgButton(GuiDialog, 101, BST_CHECKED);
        check(SendMessageTimeout(GuiDialog, WM_COMMAND, 101, 0, SMTO_ABORTIFHUNG, 5000, &Response) != 0 &&
            IsWindowEnabled(GetDlgItem(GuiDialog, 102)), "enable logging through actual GUI before editing directory");
        SendMessage(GetDlgItem(GuiDialog, 102), EM_SETSEL, 0, -1);
        check(SendMessageTimeout(GetDlgItem(GuiDialog, 102), EM_REPLACESEL, TRUE, (LPARAM)Input,
            SMTO_ABORTIFHUNG, 5000, &Response) != 0,
            "edit actual GUI directory buffer with environment Unicode and trailing slash");
        GetDlgItemText(GuiDialog, 102, Actual, ARRAYSIZE(Actual));
        // Cross-process WM_GETTEXT observes the cached caption on this isolated
        // desktop, which differs from the edit buffer changed by EM_REPLACESEL.
        fwprintf(log, L"GuiEditInput=%s CrossProcessCachedCaption=%s\n", Input, Actual);
        check(IsWindowEnabled(GetDlgItem(GuiDialog, 190)), "editing directory enables actual Apply control");
        check(PostMessage(GuiDialog, WM_COMMAND, 190, 0), "queue actual GUI Apply through ordinary message loop");
        for (Index = 0; Index < 150 && IsWindowEnabled(GetDlgItem(GuiDialog, 190)); ++Index)
            if (WaitForSingleObject(Process.hProcess, 100) == WAIT_OBJECT_0) break;
        check(!IsWindowEnabled(GetDlgItem(GuiDialog, 190)) && IsWindowEnabled(GuiDialog),
            "actual GUI Apply completes through installed runas writer");
        ExpandEnvironmentStrings(Input, Expected, ARRAYSIZE(Expected));
        KxCfgQueryLoggingSettings(&Enabled, Actual, ARRAYSIZE(Actual));
        fwprintf(log, L"SavedEnabled=%u ExpectedDir=%s ActualDir=%s\n", Enabled, Expected, Actual);
        check(KxCfgQueryLoggingSettings(&Enabled, Actual, ARRAYSIZE(Actual)) && Enabled && !wcscmp(Actual, Expected),
            "actual GUI save persists original environment-expanded directory and logging state");
        check(!IsWindowEnabled(GetDlgItem(GuiDialog, 190)), "successful GUI save clears pending changes and disables Apply");
        if (!failures) failAndRetryGui(Desktop, Process.hProcess, Expected);
        // Only close normally if the save succeeded; otherwise retain failure and
        // terminate just the owned diagnostic GUI, never dismiss unrelated windows.
        if (!failures) check(SendMessageTimeout(GuiDialog, WM_COMMAND, 192, 0, SMTO_ABORTIFHUNG, 5000, &Response) != 0,
            "saved GUI Cancel exits without unsaved-changes prompt");
    }
    if (WaitForSingleObject(Process.hProcess, 5000) != WAIT_OBJECT_0) {
        check(FALSE, "save-test GUI exits normally");
        TerminateProcess(Process.hProcess, 99); WaitForSingleObject(Process.hProcess, 5000);
    }
    check(GetExitCodeProcess(Process.hProcess, &Code) && Code == 0, "actual saved GUI exits successfully");
    CloseHandle(Process.hThread); CloseHandle(Process.hProcess);
    if (!failures) {
        check(!RegOpenKeyEx(HKEY_CURRENT_USER, PRODUCT, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Key), "open only owned operator fixture after GUI save");
        if (!failures) {
            check(!RegDeleteValue(Key, L"LogDir") && !RegWriteI32(Key, NULL, L"EnableLogging", 0),
                "restore original owned operator preferences after validated GUI save"); RegCloseKey(Key);
        }
    }
}
static void standardWriter(HANDLE Token, PCWSTR Sid)
{
    HANDLE Primary = NULL; STARTUPINFO Startup = {sizeof(Startup)};
    PROCESS_INFORMATION Process; WCHAR Command[1024]; DWORD Code = 0, Wait;
    HWINSTA Original = GetProcessWindowStation(), Station = NULL; HDESK Desktop = NULL;
    PSECURITY_DESCRIPTOR Descriptor = NULL; SECURITY_ATTRIBUTES Security = {sizeof(Security)};
    WCHAR Sddl[512];
    check(DuplicateTokenEx(Token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &Primary),
        "obtain real standard primary token for asInvoker CLI test");
    if (!Primary) return;
    // A network-logon token cannot access the operator's interactive desktop.
    // Give this command-only process its own isolated station and desktop.
    StringCchPrintf(Sddl, ARRAYSIZE(Sddl), L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;%s)", Sid);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(Sddl, SDDL_REVISION_1, &Descriptor, NULL)) {
        check(FALSE, "build isolated desktop permissions"); goto Done;
    }
    Security.lpSecurityDescriptor = Descriptor;
    Station = CreateWindowStation(L"VxKexScopeCommand261001", 0, WINSTA_ALL_ACCESS, &Security);
    if (!Station || !SetProcessWindowStation(Station)) { check(FALSE, "create private command-only window station"); goto Done; }
    Desktop = CreateDesktop(L"Default", NULL, NULL, 0, DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
        DESKTOP_WRITEOBJECTS | DESKTOP_ENUMERATE | DESKTOP_SWITCHDESKTOP, &Security);
    check(SetProcessWindowStation(Original), "restore operator window station after private desktop creation");
    if (!Desktop) { check(FALSE, "create isolated standard-user desktop"); goto Done; }
    Startup.lpDesktop = L"VxKexScopeCommand261001\\Default";
    StringCchPrintf(Command, ARRAYSIZE(Command), L"\"C:\\VxKex\\KexCfg.exe\" /GLOBAL /USER-SID:%s /LOGGING:1 /LOGDIR:C:\\UnauthorizedLogs /MSI:0 /CONTEXTMENU:0 /EXTENDED:0", Sid);
    check(privilege(SE_IMPERSONATE_NAME), "enable operator privilege for command-only standard-token launch");
    if (!CreateProcessWithTokenW(Primary, 0, L"C:\\VxKex\\KexCfg.exe", Command,
        CREATE_NO_WINDOW, NULL, L"C:\\Windows", &Startup, &Process)) {
        check(FALSE, "launch installed asInvoker CLI under real standard user without UAC");
        goto Done;
    }
    check(TRUE, "launch installed asInvoker CLI under real standard user without UAC");
    Wait = WaitForSingleObject(Process.hProcess, 30000);
    check(Wait == WAIT_OBJECT_0, "standard writer exits rather than recursively elevating");
    if (Wait != WAIT_OBJECT_0) TerminateProcess(Process.hProcess, 99);
    else check(GetExitCodeProcess(Process.hProcess, &Code) && Code == ERROR_ACCESS_DENIED,
        "unelevated actual writer returns access denied before any transaction");
    fprintf(log, "StandardWriterExit=%lu\n", Code);
    CloseHandle(Process.hThread); CloseHandle(Process.hProcess);
    if (!failures) standardGui(Primary, Desktop, &Startup, L"C:\\VxKex\\KexCfg.exe");
    if (!failures) standardGui(Primary, Desktop, &Startup, BASE L"\\KexCfg-x86.exe");
    if (!failures) saveGui(Desktop, &Startup, L"C:\\VxKex\\KexCfg.exe");
    if (!failures) saveGui(Desktop, &Startup, BASE L"\\KexCfg-x86.exe");
Done:
    if (Original) SetProcessWindowStation(Original);
    if (Desktop) CloseDesktop(Desktop); if (Station) CloseWindowStation(Station);
    if (Descriptor) LocalFree(Descriptor); CloseHandle(Primary);
}
VOID __cdecl mainCRTStartup(VOID)
{
    USER_INFO_1 account = {0}; DWORD parameter = 0, status, size, groupChars, domainChars;
    HANDLE userToken = NULL; PTOKEN_USER tokenUser = NULL; PWSTR sid = NULL;
    PROFILEINFO profile = {sizeof(profile)}; WCHAR profilePath[MAX_PATH], group[128], domain[128];
    BYTE adminSid[SECURITY_MAX_SID_SIZE], usersSid[SECURITY_MAX_SID_SIZE], accountSid[SECURITY_MAX_SID_SIZE]; BOOL admin = TRUE, owned = FALSE, loaded = FALSE, unloaded = FALSE;
    BOOL impersonating = FALSE; SID_NAME_USE use; LOCALGROUP_MEMBERS_INFO_0 member;
    HKEY key; LONG error;
    log = fopen("C:\\VxKexProbe\\NextParity\\setup-user-scope.txt", "wt"); if (!log) ExitProcess(2);
    check(guard(), "disposable VM authorization required before account or setup writes"); if (failures) goto Done;
    check(!KxCfgpElevationRequired(), "test operator is elevated");
    check(GetFileAttributes(L"C:\\VxKex") == INVALID_FILE_ATTRIBUTES && !exists(HKEY_CURRENT_USER), "fresh clone setup state and no preexisting operator preferences");
    check(GetFileAttributes(EXPECTED_PROFILE) == INVALID_FILE_ATTRIBUTES, "never reuse a preexisting test profile directory"); if (failures) goto Done;
    error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files", 0, KEY_READ | KEY_WOW64_64KEY, &key);
    check(error == ERROR_FILE_NOT_FOUND, "never adopt a preexisting cleanup handler for user-scope test"); if (!error) RegCloseKey(key); if (failures) goto Done;
    account.usri1_name = USERNAME; account.usri1_password = L"Parity_Clone_261001!";
    account.usri1_priv = USER_PRIV_USER; account.usri1_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD;
    account.usri1_comment = L"VxKex parity disposable user scope test";
    status = NetUserAdd(NULL, 1, (LPBYTE)&account, &parameter);
    fprintf(log, "CreateUserStatus=%lu Parameter=%lu\n", status, parameter);
    check(!status, "create a new standard test account without adopting an existing account"); if (status) goto Done; owned = TRUE;
    size = sizeof(usersSid); check(CreateWellKnownSid(WinBuiltinUsersSid, NULL, usersSid, &size), "obtain localized Users group SID");
    groupChars = ARRAYSIZE(group); domainChars = ARRAYSIZE(domain);
    check(LookupAccountSid(NULL, usersSid, group, &groupChars, domain, &domainChars, &use), "resolve localized Users group"); if (failures) goto Done;
    size = sizeof(accountSid); domainChars = ARRAYSIZE(domain);
    check(LookupAccountName(NULL, USERNAME, accountSid, &size, domain, &domainChars, &use), "resolve newly created account SID"); if (failures) goto Done;
    member.lgrmi0_sid = (PSID)accountSid;
    status = NetLocalGroupAddMembers(NULL, group, 0, (LPBYTE)&member, 1);
    fprintf(log, "AddUsersGroupStatus=%lu\n", status);
    check(!status || status == ERROR_MEMBER_IN_ALIAS, "test account belongs to ordinary Users group");
    check(LogonUser(USERNAME, L".", account.usri1_password, LOGON32_LOGON_NETWORK, LOGON32_PROVIDER_DEFAULT, &userToken), "authenticate standard account without interactive logon"); if (failures) goto Done;
    size = sizeof(adminSid); check(CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, adminSid, &size), "obtain administrator group SID");
    check(CheckTokenMembership(userToken, adminSid, &admin) && !admin, "standard token is not an administrator");
    size = 0; GetTokenInformation(userToken, TokenUser, NULL, 0, &size); tokenUser = HeapAlloc(GetProcessHeap(), 0, size);
    check(tokenUser && GetTokenInformation(userToken, TokenUser, tokenUser, size, &size) && ConvertSidToStringSid(tokenUser->User.Sid, &sid), "capture real initiating standard-user SID");
    check(privilege(SE_BACKUP_NAME) && privilege(SE_RESTORE_NAME), "enable documented profile-loading privileges on test operator"); if (failures) goto Done;
    profile.dwFlags = PI_NOUI; profile.lpUserName = USERNAME;
    loaded = LoadUserProfile(userToken, &profile); check(loaded, "load real standard-user hive without UI"); if (!loaded) goto Done;
    size = ARRAYSIZE(profilePath);
    check(GetUserProfileDirectory(userToken, profilePath, &size) && !_wcsicmp(profilePath, EXPECTED_PROFILE), "resolved profile deletion target is exactly the newly authorized account directory"); if (failures) goto Done;
    preference(HKEY_CURRENT_USER, 0, TRUE); preference(profile.hProfile, 1, TRUE); if (failures) goto Done;
    impersonating = ImpersonateLoggedOnUser(userToken); check(impersonating, "impersonate actual standard-user token for permission checks");
    if (impersonating) {
        check(KxCfgpElevationRequired(), "standard identity requires elevation in the native setup access check");
        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
            0, KEY_WRITE | KEY_WOW64_64KEY, &key);
        check(error == ERROR_ACCESS_DENIED, "standard identity cannot write machine compatibility configuration"); if (!error) RegCloseKey(key);
        check(RevertToSelf(), "restore operator identity"); impersonating = FALSE;
    }
    if (failures) goto Done;
    check(!setup(TRUE, sid), "elevated install accepts explicit different initiating SID"); if (failures) goto Done;
    standardWriter(userToken, sid); if (failures) goto Done;
    preference(HKEY_CURRENT_USER, 0, FALSE); preference(profile.hProfile, 1, FALSE); if (failures) goto Done;
    check(!globalSettings(sid), "elevated global CLI writes to actual loaded standard-user SID");
    preference(HKEY_CURRENT_USER, 0, FALSE); preference(profile.hProfile, 0, FALSE);
    error = RegOpenKeyEx(profile.hProfile, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    check(!error, "open real selected standard-user log directory preference");
    if (!error) {
        WCHAR logDir[MAX_PATH];
        check(!RegReadString(key, NULL, L"LogDir", logDir, ARRAYSIZE(logDir)) && !wcscmp(logDir, L"C:\\SelectedStandardUserLogs"),
            "real standard-user hive receives resolved log directory without operator-environment substitution"); RegCloseKey(key);
    }
    error = RegOpenKeyEx(HKEY_CURRENT_USER, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (!error) {
        WCHAR logDir[MAX_PATH];
        check(RegReadString(key, NULL, L"LogDir", logDir, ARRAYSIZE(logDir)) == ERROR_FILE_NOT_FOUND, "operator log-directory preference remains absent"); RegCloseKey(key);
    } else check(FALSE, "open operator preferences after global CLI");
    impersonating = ImpersonateLoggedOnUser(userToken); check(impersonating, "impersonate standard user after elevated global save");
    if (impersonating) {
        HKEY currentUser; WCHAR logDir[MAX_PATH];
        error = RegOpenCurrentUser(KEY_READ, &currentUser);
        check(!error, "open uncached actual standard-user HKCU under impersonation");
        if (!error) {
            error = RegOpenKeyEx(currentUser, PRODUCT, 0, KEY_READ | KEY_WOW64_64KEY, &key);
            check(!error, "standard user can read settings created by elevated helper");
            if (!error) {
                check(!RegReadString(key, NULL, L"LogDir", logDir, ARRAYSIZE(logDir)) && !wcscmp(logDir, L"C:\\SelectedStandardUserLogs"),
                    "standard user's own identity sees its saved directory"); RegCloseKey(key);
            }
            RegCloseKey(currentUser);
        }
        check(RevertToSelf(), "restore administrator after standard-user read check"); impersonating = FALSE;
    }
    if (failures) goto Done;
    check(!setup(FALSE, sid), "elevated remove-all completes for explicit initiating SID"); if (failures) goto Done;
    check(!exists(profile.hProfile), "only initiating standard-user preferences are removed");
    preference(HKEY_CURRENT_USER, 0, FALSE);
    check(GetFileAttributes(L"C:\\VxKex") == INVALID_FILE_ATTRIBUTES, "real installation removed");
    if (!failures) check(!RegDeleteKeyEx(HKEY_CURRENT_USER, PRODUCT, KEY_WOW64_64KEY, 0), "clean only operator preferences created by this test");
    if (!failures) check(!RegDeleteTree(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files"),
        "remove only global-CLI cleanup fixture whose absence was verified before test");
Done:
    if (impersonating) RevertToSelf();
    if (loaded) { unloaded = UnloadUserProfile(userToken, profile.hProfile); check(unloaded, "unload standard-user hive after all helper processes exit"); }
    if (!failures && unloaded && sid && !_wcsicmp(profilePath, EXPECTED_PROFILE)) {
        check(DeleteProfile(sid, profilePath, NULL), "delete only verified new test profile directory");
        if (!failures && owned) check(!NetUserDel(NULL, USERNAME), "delete only account created by this test");
    }
    if (sid) LocalFree(sid); if (tokenUser) HeapFree(GetProcessHeap(), 0, tokenUser); if (userToken) CloseHandle(userToken);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
