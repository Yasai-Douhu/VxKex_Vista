// Native Vista launcher. IFEO mode detaches before the target's entry point.
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include "../../00-Common-Headers/VistaLaunch.h"

static void fail(LPCWSTR operation, DWORD error) {
    WCHAR text[256];
    wsprintfW(text, L"%s failed (0x%08X).", operation, error);
    MessageBoxW(NULL, text, L"VxKex Vista launcher", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}
static WCHAR *skipExecutable(WCHAR *p) {
    if (*p == L'"') { ++p; while (*p && *p != L'"') ++p; if (*p) ++p; }
    else while (*p && *p != L' ' && *p != L'\t') ++p;
    return p;
}
static LONG removeLaunchers(HKEY key) {
    DWORD index = 0, size; WCHAR name[256]; HKEY child; LONG error;
    error = VistaRemoveManagedDebugger(key);
    if (error) return error;
    for (;;) {
        size = 256;
        error = RegEnumKeyExW(key, index++, name, &size, NULL, NULL, NULL, NULL);
        if (error == ERROR_NO_MORE_ITEMS) return ERROR_SUCCESS;
        if (error) return error;
        error = RegOpenKeyExW(key, name, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &child);
        if (error) return error;
        error = removeLaunchers(child); RegCloseKey(child);
        if (error) return error;
    }
}
void mainCRTStartup(void) {
    WCHAR dllPath[MAX_PATH], target[MAX_PATH], *args, *command, *end;
    HMODULE kex; LONG (WINAPI *patch)(void); LONG status;
    UINT length, n; BOOL ifeo = FALSE, code = FALSE;
    static STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process;
    static const WCHAR flags[] = L" --no-sandbox --disable-gpu --disable-gpu-sandbox --disable-software-rasterizer --use-gl=disabled";

    args = skipExecutable(GetCommandLineW());
    while (*args == L' ' || *args == L'\t') ++args;
    if (!lstrcmpW(args, L"--remove-launchers")) {
        HKEY key; LONG error = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
            0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key);
        if (!error) { error = removeLaunchers(key); RegCloseKey(key); }
        ExitProcess(error == ERROR_FILE_NOT_FOUND ? 0 : error);
    }
    if (args[0] == L'-' && args[1] == L'-' && args[2] == L'i' && args[3] == L'f' &&
        args[4] == L'e' && args[5] == L'o' && (args[6] == L' ' || args[6] == L'\t')) {
        ifeo = TRUE; args += 7; while (*args == L' ' || *args == L'\t') ++args;
    }
    if (!*args) fail(L"Missing application path", ERROR_INVALID_PARAMETER);
    end = skipExecutable(args);
    n = (UINT)(end - args); length = n;
    if (*args == L'"') { if (n < 2) fail(L"Application path", ERROR_INVALID_PARAMETER); n -= 2; }
    if (n >= MAX_PATH) fail(L"Application path", ERROR_FILENAME_EXCED_RANGE);
    { UINT i; WCHAR *start = args + (*args == L'"'); for(i = 0; i < n; ++i) target[i] = start[i]; target[n] = 0; }
    code = ifeo && VistaIsVSCode(target);
    length = GetSystemDirectoryW(dllPath, MAX_PATH);
    if (!length || length + 12 >= MAX_PATH) fail(L"GetSystemDirectory", GetLastError());
    lstrcatW(dllPath, L"\\KexDll.dll");
    kex = LoadLibraryW(dllPath);
    if (!kex) fail(L"Load KexDll.dll", GetLastError());
    patch = (void *)GetProcAddress(kex, "KexPatchCpiwSubsystemVersionCheck");
    if (!patch) fail(L"Find subsystem compatibility function", GetLastError());
    status = patch();
    if (status < 0) fail(L"Subsystem compatibility initialization", (DWORD)status);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    length = lstrlenW(args) + (code ? lstrlenW(flags) : 0) + 1;
    if (length > 32767) fail(L"Command line", ERROR_FILENAME_EXCED_RANGE);
    command = HeapAlloc(GetProcessHeap(), 0, length * sizeof(WCHAR));
    if (!command) fail(L"Allocate command line", ERROR_NOT_ENOUGH_MEMORY);
    // Insert switches before user arguments, including a possible "--".
    { UINT i; for(i = 0; args + i < end; ++i) command[i] = args[i]; command[i] = 0; }
    if (code) lstrcatW(command, flags);
    lstrcatW(command, end);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE,
        ifeo ? DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED : 0,
        NULL, NULL, &startup, &process)) fail(L"CreateProcess", GetLastError());
    if (ifeo) {
        if (!DebugSetProcessKillOnExit(FALSE) || !DebugActiveProcessStop(process.dwProcessId)) {
            DWORD error = GetLastError(); TerminateProcess(process.hProcess, error); fail(L"Detach", error);
        }
        if (ResumeThread(process.hThread) == (DWORD)-1) {
            DWORD error = GetLastError(); TerminateProcess(process.hProcess, error); fail(L"Resume", error);
        }
    }
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    HeapFree(GetProcessHeap(), 0, command); ExitProcess(0);
}
