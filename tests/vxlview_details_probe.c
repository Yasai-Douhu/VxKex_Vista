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
static HDESK desktop;
static HWND gotoWindow, details;
static BOOL CALLBACK FindGoto(HWND window, LPARAM unused)
{
    DWORD pid; GetWindowThreadProcessId(window, &pid);
    if (pid == target && GetWindow(window, GW_OWNER) == mainWindow && GetDlgItem(window, 0x480)) {
        gotoWindow=window; return FALSE;
    }
    return TRUE;
}
static BOOL CALLBACK FindDetails(HWND window, LPARAM unused)
{
    if (GetDlgItem(window, 152)) { details=window; return FALSE; } return TRUE;
}
static BOOL readEdit(HWND edit, PWSTR text, unsigned capacity)
{
    DWORD_PTR lines, length; unsigned index, used=0;
    text[0]=0;
    if(!SendMessageTimeoutW(edit,EM_GETLINECOUNT,0,0,SMTO_ABORTIFHUNG,5000,&lines)) return FALSE;
    for(index=0;index<lines;++index) {
        WCHAR line[2048]; *(WORD*)line=2047;
        if(!SendMessageTimeoutW(edit,EM_GETLINE,index,(LPARAM)line,SMTO_ABORTIFHUNG,5000,&length) || length>2047 || used+length+2>capacity) return FALSE;
        CopyMemory(text+used,line,length*sizeof(WCHAR)); used+=(unsigned)length; text[used++]=L'\n'; text[used]=0;
    }
    return TRUE;
}
static HWND openGoto(void)
{
    int index; gotoWindow=NULL; PostMessageW(mainWindow, WM_COMMAND, 231, 0);
    for(index=0; index<100 && !gotoWindow; ++index) { EnumDesktopWindows(desktop, FindGoto, 0); Sleep(20); }
    if(!gotoWindow) { fprintf(log,"Goto dialog missing\n"); ++failures; } return gotoWindow;
}
static void gotoValue(PCWSTR value, BOOL accepted)
{
    HWND dialog=openGoto(), edit; DWORD_PTR result; int index;
    if(!dialog) return; edit=GetDlgItem(dialog,0x480);
    SendMessageTimeoutW(edit, EM_SETSEL, 0, -1, SMTO_ABORTIFHUNG, 5000, &result);
    SendMessageTimeoutW(edit, EM_REPLACESEL, FALSE, (LPARAM)value, SMTO_ABORTIFHUNG, 5000, &result);
    PostMessageW(dialog, WM_COMMAND, IDOK, 0);
    if(accepted) { for(index=0;index<100 && IsWindow(dialog); ++index) Sleep(20); }
    else Sleep(200);
    fprintf(log,"Goto=%ls Accepted=%d ActualClosed=%d\n",value,accepted,!IsWindow(dialog));
    if(accepted == IsWindow(dialog)) ++failures;
    if(IsWindow(dialog)) { PostMessageW(dialog,WM_CLOSE,0,0); for(index=0;index<100 && IsWindow(dialog);++index) Sleep(20); }
}
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
    HWINSTA station, originalStation; HDESK originalDesktop; WCHAR stationName[80], desktopName[100];
    if (argc != 4 && !(argc == 5 && !wcscmp(argv[4], L"/association"))) return 2;
    log = _wfopen(argv[3], L"wt");
    if (!log) return 3;
    setbuf(log, NULL);
    // All owned windows stay on a private desktop that is never displayed.
    originalStation=GetProcessWindowStation(); originalDesktop=GetThreadDesktop(GetCurrentThreadId());
    swprintf_s(stationName,80,L"VxlDetailsProbe%lu",GetCurrentProcessId());
    station=CreateWindowStationW(stationName,0,WINSTA_ALL_ACCESS,NULL);
    if(!station || !SetProcessWindowStation(station)) return 7;
    desktop=CreateDesktopW(L"Default",NULL,NULL,0,GENERIC_ALL,NULL);
    if(!desktop || !SetThreadDesktop(desktop)) return 8;
    swprintf_s(desktopName,100,L"%s\\Default",stationName); startup.lpDesktop=desktopName;
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
        EnumDesktopWindows(desktop, FindMain, 0);
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
    gotoValue(L"0",FALSE);
    gotoValue(L"7",FALSE);
    gotoValue(L"6",TRUE);
    EnumChildWindows(mainWindow,FindDetails,0);
    if(details) {
        WCHAR text[2048], source[512], severity[256], time[256];
        if(!readEdit(GetDlgItem(details,152),text,2048)) ++failures;
        GetDlgItemTextW(details,155,source,512);
        GetDlgItemTextW(details,153,severity,256); GetDlgItemTextW(details,154,time,256);
        { char utf8[8192]; WideCharToMultiByte(CP_UTF8,0,text,-1,utf8,sizeof(utf8),NULL,NULL); fprintf(log,"ActualDetails=%s\n",utf8); }
        fprintf(log,"DetailsTextMatches=%d SourceMatches=%d MetadataPresent=%d\n",
            wcsstr(text,L"record 5") && wcsstr(text,L"\x65e5\x672c\x8a9e 5"),
            wcsstr(source,L"fixture.c") && wcsstr(source,L"105") && wcsstr(source,L"WriteFixture"), *severity && *time);
        if(!wcsstr(text,L"record 5") || !wcsstr(text,L"\x65e5\x672c\x8a9e 5") ||
            !wcsstr(source,L"fixture.c") || !wcsstr(source,L"105") || !wcsstr(source,L"WriteFixture") || !*severity || !*time) ++failures;
    } else { fprintf(log,"Details missing\n"); ++failures; }
    setOption(filter,112,FALSE); gotoValue(L"3",FALSE);
    setOption(filter,112,TRUE); gotoValue(L"3",TRUE);
    if(details) { WCHAR text[2048]; if(!readEdit(GetDlgItem(details,152),text,2048) || !wcsstr(text,L"Warning Alpha")) ++failures; }
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
    SetThreadDesktop(originalDesktop); SetProcessWindowStation(originalStation);
    CloseDesktop(desktop); CloseWindowStation(station);
    return failures || exitCode != 0;
}
