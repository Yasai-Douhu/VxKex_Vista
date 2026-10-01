/* Command-based smoke test: launches only the specified test build and reads
   its controls. Does not apply or change any user configuration. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static DWORD target;
static HWND dialog;
static BOOL CALLBACK FindDialog(HWND window, LPARAM unused)
{
    DWORD pid;
    WCHAR name[64];
    GetWindowThreadProcessId(window, &pid);
    GetClassNameW(window, name, 64);
    if (pid == target && !lstrcmpW(name, L"#32770")) {
        dialog = window;
        return FALSE;
    }
    return TRUE;
}
int wmain(int argc, WCHAR **argv)
{
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process;
    WCHAR command[1024], title[256];
    FILE *log;
    int i, failed = 0;
    DWORD exitCode = 1;
    const int ids[] = {101,102,103,110,111,112,113,120,121,122,123,124,190,191,192};
    if (argc != 3) return 2;
    log = _wfopen(argv[2], L"wt");
    if (!log) return 3;
    setbuf(log, NULL);
    if (swprintf_s(command, 1024, L"\"%s\"", argv[1]) < 0) return 4;
    if (!CreateProcessW(argv[1], command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        fprintf(log, "CreateProcess error=%lu\n", GetLastError());
        fclose(log);
        return 5;
    }
    target = process.dwProcessId;
    WaitForInputIdle(process.hProcess, 10000);
    for (i = 0; i < 100 && !dialog; ++i) {
        EnumWindows(FindDialog, 0);
        if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0) break;
    }
    if (!dialog) { fprintf(log, "No main dialog\n"); failed = 1; }
    else {
        LRESULT count;
        DWORD_PTR response;
        GetWindowTextW(dialog, title, 256);
        fwprintf(log, L"Title=%s\n", title);
        for (i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i) {
            HWND control = GetDlgItem(dialog, ids[i]);
            fprintf(log, "Control[%d]=%d enabled=%d\n", ids[i], !!control,
                control ? IsWindowEnabled(control) : 0);
            if (!control) failed = 1;
        }
        count = SendMessageW(GetDlgItem(dialog, 120), LVM_GETITEMCOUNT, 0, 0);
        fprintf(log, "ApplicationCount=%ld\n", (long)count);
        if (count < 1 || IsWindowEnabled(GetDlgItem(dialog, 111))) failed = 1;
        if (!SendMessageTimeoutW(dialog, WM_COMMAND, 192, 0, SMTO_ABORTIFHUNG, 5000, &response)) {
            fprintf(log, "Cancel timed out\n"); failed = 1;
        }
    }
    if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0) {
        fprintf(log, "Process did not close\n"); failed = 1;
        TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, 5000);
    }
    GetExitCodeProcess(process.hProcess, &exitCode);
    fprintf(log, "ChildExit=%lu ProbeResult=%d\n", exitCode, failed);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    fclose(log);
    return failed || exitCode != 0;
}
