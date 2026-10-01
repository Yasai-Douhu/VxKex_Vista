#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
static DWORD target;
static HWND mainWindow, search;
static FILE *log;
static int failures;
static BOOL CALLBACK FindMain(HWND window, LPARAM unused)
{
    DWORD pid;
    GetWindowThreadProcessId(window, &pid);
    if (pid == target && GetDlgItem(window, 102) && GetDlgItem(window, 103)) {
        mainWindow = window; return FALSE;
    }
    return TRUE;
}
static BOOL CALLBACK FindSearch(HWND window, LPARAM unused)
{
    if (GetDlgCtrlID(window) == 121) { search = window; return FALSE; }
    return TRUE;
}
static void expectCount(int expected, const char *name)
{
    DWORD_PTR count = 0;
    BOOL success = SendMessageTimeoutW(GetDlgItem(mainWindow, 102), LVM_GETITEMCOUNT,
        0, 0, SMTO_ABORTIFHUNG, 5000, &count) != 0;
    fprintf(log, "%s count=%lu expected=%d\n", name, (unsigned long)count, expected);
    if (!success || count != expected) ++failures;
}
static void setOption(HWND filter, int id, BOOL checked)
{
    HWND control = GetDlgItem(filter, id);
    DWORD_PTR response;
    if (!control) { fprintf(log, "Missing option %d\n", id); ++failures; return; }
    SendMessageTimeoutW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED,
        0, SMTO_ABORTIFHUNG, 5000, &response);
    SendMessageTimeoutW(filter, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
        (LPARAM)control, SMTO_ABORTIFHUNG, 5000, &response);
}
int wmain(int argc, WCHAR **argv)
{
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    WCHAR command[2048], title[512];
    DWORD_PTR response;
    DWORD exitCode;
    int index;
    HWND filter, warning;
    if (argc != 4 && !(argc == 5 && !wcscmp(argv[4], L"/association"))) return 2;
    log = _wfopen(argv[3], L"wt");
    if (!log) return 3;
    setbuf(log, NULL);
    if (swprintf_s(command, 2048, L"\"%s\" \"%s\"", argv[1], argv[2]) < 0) return 4;
    if (argc == 5) {
        SHELLEXECUTEINFOW launch = {sizeof(launch)};
        WCHAR actualImage[1024];
        DWORD imageLength = 1024;
        launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        launch.lpVerb = L"open"; launch.lpFile = argv[2]; launch.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&launch) || !launch.hProcess) {
            fprintf(log, "Association launch error=%lu\n", GetLastError()); fclose(log); return 5;
        }
        process.hProcess = launch.hProcess;
        process.dwProcessId = GetProcessId(launch.hProcess);
        if (!QueryFullProcessImageNameW(launch.hProcess, 0, actualImage, &imageLength) ||
            _wcsicmp(actualImage, argv[1])) {
            fprintf(log, "Association launched a different image\n");
            CloseHandle(launch.hProcess); fclose(log); return 6;
        }
        fprintf(log, "Association launched expected image\n");
    } else if (!CreateProcessW(argv[1], command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        fprintf(log, "CreateProcess error=%lu\n", GetLastError()); fclose(log); return 5;
    }
    target = process.dwProcessId;
    WaitForInputIdle(process.hProcess, 10000);
    for (index = 0; index < 100 && !mainWindow; ++index) {
        EnumWindows(FindMain, 0);
        if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0) break;
    }
    if (!mainWindow) { fprintf(log, "Main window not found\n"); ++failures; goto Close; }
    GetWindowTextW(mainWindow, title, 512);
    fwprintf(log, L"Title=%s\n", title);
    EnumChildWindows(mainWindow, FindSearch, 0);
    if (!search) { fprintf(log, "Search control not found\n"); ++failures; goto Close; }
    expectCount(6, "Initial");
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"Alpha", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(1, "Search Alpha");
    filter = GetParent(search);
    setOption(filter, 124, TRUE);
    expectCount(5, "Invert Alpha");
    setOption(filter, 124, FALSE);
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"alpha", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(1, "Case insensitive alpha");
    setOption(filter, 122, TRUE);
    expectCount(0, "Case sensitive alpha");
    setOption(filter, 122, FALSE);
    setOption(filter, 123, TRUE);
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"*Alpha*", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(1, "Wildcard Alpha");
    setOption(filter, 123, FALSE);
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"\x65e5\x672c\x8a9e", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(0, "Japanese header only");
    setOption(filter, 126, TRUE);
    expectCount(6, "Japanese body search");
    setOption(filter, 126, FALSE);
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"NoSuchFixtureText", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(0, "Search absent");
    SendMessageTimeoutW(search, WM_SETTEXT, 0, (LPARAM)L"", SMTO_ABORTIFHUNG, 5000, &response);
    expectCount(6, "Clear search");
    filter = GetParent(search);
    warning = GetDlgItem(filter, 112);
    if (!warning) { fprintf(log, "Warning checkbox not found\n"); ++failures; goto Close; }
    setOption(filter, 112, FALSE);
    expectCount(5, "Hide warning");
    setOption(filter, 112, TRUE);
    expectCount(6, "Restore warning");
Close:
    if (mainWindow) SendMessageTimeoutW(mainWindow, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 5000, &response);
    if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0) {
        fprintf(log, "Process did not close\n"); ++failures;
        TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, 5000);
    }
    GetExitCodeProcess(process.hProcess, &exitCode);
    fprintf(log, "Exit=%lu Failures=%d\n", exitCode, failures);
    if (process.hThread) CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    fclose(log);
    return failures || exitCode != 0;
}
