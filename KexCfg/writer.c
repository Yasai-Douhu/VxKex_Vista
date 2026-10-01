#include "kexcfg.h"

// Quote using CommandLineToArgvW rules, including a trailing backslash.
BOOL KexCfgAppendArgument(PWSTR Command, SIZE_T Capacity, PCWSTR Argument)
{
    SIZE_T Used = wcslen(Command), Slashes, Index;
    PCWSTR Cursor = Argument;
    if (Used + 3 >= Capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    Command[Used++] = L' '; Command[Used++] = L'"';
    for (;;) {
        Slashes = 0;
        while (*Cursor == L'\\') { ++Slashes; ++Cursor; }
        if (!*Cursor || *Cursor == L'"') Slashes *= 2;
        for (Index = 0; Index < Slashes; ++Index) {
            if (Used + 3 >= Capacity) goto Fail;
            Command[Used++] = L'\\';
        }
        if (!*Cursor) break;
        if (*Cursor == L'"') {
            if (Used + 3 >= Capacity) goto Fail;
            Command[Used++] = L'\\';
        }
        if (Used + 3 >= Capacity) goto Fail;
        Command[Used++] = *Cursor++;
    }
    Command[Used++] = L'"'; Command[Used] = 0; return TRUE;
Fail:
    Command[Used] = 0; SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE;
}

DWORD KexCfgRunWriter(HWND Owner, PCWSTR Arguments)
{
    WCHAR Writer[MAX_PATH]; SHELLEXECUTEINFO Execute = {sizeof(Execute)};
    DWORD Result, Wait; MSG Message; BOOL Quit = FALSE; WPARAM QuitCode = 0;
    // The installed native executable manages both physical MSI images.
    if (!KxCfgGetKexDir(Writer, ARRAYSIZE(Writer))) return GetLastError();
    if (FAILED(PathCchAppend(Writer, ARRAYSIZE(Writer), L"KexCfg.exe"))) return ERROR_BAD_PATHNAME;
    Execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    Execute.hwnd = Owner; Execute.lpVerb = L"runas";
    Execute.lpFile = Writer; Execute.lpParameters = Arguments; Execute.nShow = SW_HIDE;
    if (!ShellExecuteEx(&Execute)) return GetLastError();
    if (!Execute.hProcess) return ERROR_INVALID_HANDLE;
    if (Owner) EnableWindow(Owner, FALSE);
    for (;;) {
        Wait = MsgWaitForMultipleObjects(1, &Execute.hProcess, FALSE, INFINITE, QS_ALLINPUT);
        if (Wait == WAIT_OBJECT_0) break;
        if (Wait == WAIT_FAILED) { Result = GetLastError(); goto Done; }
        while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) { Quit = TRUE; QuitCode = Message.wParam; }
            else { TranslateMessage(&Message); DispatchMessage(&Message); }
        }
    }
    if (!GetExitCodeProcess(Execute.hProcess, &Result)) Result = GetLastError();
Done:
    CloseHandle(Execute.hProcess);
    if (Owner && IsWindow(Owner)) EnableWindow(Owner, TRUE);
    if (Quit) PostQuitMessage((int)QuitCode);
    return Result;
}
