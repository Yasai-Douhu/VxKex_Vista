#define _WIN32_WINNT 0x0600
#include <windows.h>

static void writeResult(const WCHAR *message) {
    DWORD written;
    WCHAR path[MAX_PATH];
    HANDLE file;
    if (!GetTempPathW(MAX_PATH, path) ||
            lstrlenW(path) + 25 >= MAX_PATH) return;
    lstrcatW(path, L"vistarun-wait-result.txt");
    file = CreateFileW(path,
        GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, message, lstrlenW(message) * sizeof(WCHAR), &written, NULL);
        CloseHandle(file);
    }
}

int main(void) {
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process;
    DWORD wait, initialWait, exitCode = 0;
    WCHAR output[128];
    WCHAR command[] = L"\"C:\\VxKexProbe\\vistarun_wait_child.exe\"";
    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, 0,
            NULL, NULL, &startup, &process)) {
        wsprintfW(output, L"CreateProcess failed: %lu", GetLastError());
        writeResult(output);
        return 1;
    }
    CloseHandle(process.hThread);
    initialWait = WaitForSingleObject(process.hProcess, 500);
    wait = initialWait;
    if (wait == WAIT_TIMEOUT) wait = WaitForSingleObject(process.hProcess, 60000);
    if (wait != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &exitCode)) {
        wsprintfW(output, L"Wait failed: %lu, error: %lu", wait, GetLastError());
        writeResult(output);
        CloseHandle(process.hProcess);
        return 1;
    }
    wsprintfW(output, L"initial wait: %lu; observed exit: %lu", initialWait, exitCode);
    writeResult(output);
    CloseHandle(process.hProcess);
    return initialWait == WAIT_TIMEOUT && exitCode == 37 ? 0 : 1;
}
