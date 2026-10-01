#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <stdio.h>

// Observe the real installation while the external CLI runs check-only modes.
// Live Logs are excluded: running applications may append to them independently.
static FILE *log;
static unsigned failures;
static void check(BOOL ok, PCSTR message)
{
    fprintf(log, "%s %s\n", ok ? "PASS" : "FAIL", message);
    if (!ok) ++failures;
}
static void bytes(PULONGLONG hash, const void *data, DWORD length)
{
    PCBYTE p = data;
    while (length--) { *hash ^= *p++; *hash *= 1099511628211ui64; }
}
static LONG registry(HKEY key, REGSAM view, PULONGLONG hash, unsigned depth)
{
    WCHAR name[256]; BYTE *data; DWORD i, chars, size, type; HKEY child; LONG error;
    if (depth > 32) return ERROR_INVALID_DATA;
    data = HeapAlloc(GetProcessHeap(), 0, 1024 * 1024);
    if (!data) return ERROR_NOT_ENOUGH_MEMORY;
    for (i = 0;; ++i) {
        chars = ARRAYSIZE(name); size = 1024 * 1024;
        error = RegEnumValue(key, i, name, &chars, NULL, &type, data, &size);
        if (error == ERROR_NO_MORE_ITEMS) break;
        if (error) goto Done;
        bytes(hash, name, (chars + 1) * sizeof(WCHAR));
        bytes(hash, &type, sizeof(type)); bytes(hash, &size, sizeof(size)); bytes(hash, data, size);
    }
    for (i = 0;; ++i) {
        chars = ARRAYSIZE(name);
        error = RegEnumKeyEx(key, i, name, &chars, NULL, NULL, NULL, NULL);
        if (error == ERROR_NO_MORE_ITEMS) { error = 0; break; }
        if (error) goto Done;
        bytes(hash, name, (chars + 1) * sizeof(WCHAR));
        error = RegOpenKeyEx(key, name, 0, KEY_READ | view, &child);
        if (error) goto Done;
        error = registry(child, view, hash, depth + 1); RegCloseKey(child);
        if (error) goto Done;
    }
Done:
    HeapFree(GetProcessHeap(), 0, data); return error;
}
static LONG registryPath(HKEY root, PCWSTR path, REGSAM view, PULONGLONG hash)
{
    HKEY key; LONG error = RegOpenKeyEx(root, path, 0, KEY_READ | view, &key);
    bytes(hash, path, (DWORD)(wcslen(path) + 1) * sizeof(WCHAR));
    bytes(hash, &error, sizeof(error));
    if (error == ERROR_FILE_NOT_FOUND) return 0;
    if (error) return error;
    error = registry(key, view, hash, 0); RegCloseKey(key); return error;
}
static LONG files(PCWSTR path, PULONGLONG hash, unsigned depth)
{
    WCHAR child[MAX_PATH]; WIN32_FIND_DATA find; HANDLE h; DWORD attributes, got, error; BYTE buffer[16384];
    if (depth > 32) return ERROR_INVALID_DATA;
    attributes = GetFileAttributes(path);
    bytes(hash, path, (DWORD)(wcslen(path) + 1) * sizeof(WCHAR));
    bytes(hash, &attributes, sizeof(attributes));
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 0 : error;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_NOT_SUPPORTED;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (h == INVALID_HANDLE_VALUE) return GetLastError();
        error = 0;
        for (;;) {
            if (!ReadFile(h, buffer, sizeof(buffer), &got, NULL)) { error = GetLastError(); break; }
            if (!got) break;
            bytes(hash, buffer, got);
        }
        CloseHandle(h); return error;
    }
    if (FAILED(StringCchPrintf(child, ARRAYSIZE(child), L"%s\\*", path))) return ERROR_BAD_PATHNAME;
    h = FindFirstFile(child, &find);
    if (h == INVALID_HANDLE_VALUE) { error = GetLastError(); return error == ERROR_FILE_NOT_FOUND ? 0 : error; }
    error = 0;
    do {
        if (!wcscmp(find.cFileName, L".") || !wcscmp(find.cFileName, L"..")) continue;
        if (!depth && !_wcsicmp(find.cFileName, L"Logs")) continue;
        if (FAILED(StringCchPrintf(child, ARRAYSIZE(child), L"%s\\%s", path, find.cFileName))) { error = ERROR_BAD_PATHNAME; break; }
        error = files(child, hash, depth + 1);
        if (error) break;
    } while (FindNextFile(h, &find));
    if (!error && GetLastError() != ERROR_NO_MORE_FILES) error = GetLastError();
    FindClose(h); return error;
}
static LONG state(PULONGLONG hash)
{
    static PCWSTR machine[] = {
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
        L"Software\\VXsoft",
        L"Software\\Classes\\CLSID\\{9AACA888-A5F5-4C01-852E-8A2005C1D45F}",
        L"Software\\Classes\\exefile\\shellex\\PropertySheetHandlers",
        L"Software\\Classes\\lnkfile\\shellex\\PropertySheetHandlers",
        L"Software\\Classes\\.vxl", L"Software\\Classes\\VxKexVista.Log",
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"
    };
    static PCWSTR libs[] = {L"KexDll.dll", L"KxBase.dll", L"KxNt.dll", L"KxAdvapi.dll", L"KxCom.dll",
        L"KxCrt.dll", L"KxCryp.dll", L"KxDw.dll", L"KxDx.dll", L"KxMi.dll", L"KxNet.dll", L"KxUia.dll", L"KxUser.dll", L"KxSChanl.dll"};
    WCHAR windows[MAX_PATH], path[MAX_PATH]; unsigned i, view; LONG error;
    *hash = 14695981039346656037ui64;
    for (view = 0; view < 2; ++view) for (i = 0; i < ARRAYSIZE(machine); ++i) {
        error = registryPath(HKEY_LOCAL_MACHINE, machine[i], view ? KEY_WOW64_32KEY : KEY_WOW64_64KEY, hash);
        if (error) return error;
    }
    error = registryPath(HKEY_CURRENT_USER, L"Software\\VXsoft", KEY_WOW64_64KEY, hash);
    if (error) return error;
    error = files(L"C:\\VxKex", hash, 0); if (error) return error;
    i = GetWindowsDirectory(windows, ARRAYSIZE(windows));
    if (!i || i >= ARRAYSIZE(windows)) return ERROR_BAD_PATHNAME;
    for (view = 0; view < 2; ++view) for (i = 0; i < ARRAYSIZE(libs); ++i) {
        if (FAILED(StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s\\%s", windows,
            view ? L"SysWOW64" : L"System32", libs[i]))) return ERROR_BAD_PATHNAME;
        error = files(path, hash, 0); if (error) return error;
    }
    return 0;
}
VOID __cdecl mainCRTStartup(VOID)
{
    WCHAR command[] = L"cmd.exe /c C:\\VxKexProbe\\NextParity\\run-vistasetup-cli-checks.cmd";
    STARTUPINFO startup = {sizeof(startup)}; PROCESS_INFORMATION process; ULONGLONG before, after;
    DWORD exitCode = 1, error; BOOL launched;
    log = fopen("C:\\VxKexProbe\\NextParity\\vistasetup-rollback-result.txt", "wt");
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    error = state(&before); check(!error, "capture real installation before CLI checks");
    fprintf(log, "BeforeStatus=%lu Hash=%016I64x\n", error, before);
    if (!error) {
        launched = CreateProcess(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
        check(launched, "run external check-only CLI harness");
        if (launched) {
            WaitForSingleObject(process.hProcess, INFINITE);
            check(GetExitCodeProcess(process.hProcess, &exitCode) && !exitCode, "CLI harness completes");
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
        }
        error = state(&after); check(!error, "capture real installation after CLI checks");
        fprintf(log, "AfterStatus=%lu Hash=%016I64x\n", error, after);
        check(!error && before == after, "registry values and product files unchanged after CLI rollback");
    }
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
