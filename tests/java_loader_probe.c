#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>

typedef LONG (NTAPI *PRtlGetLastNtStatus)(void);

int wmain(int argc, wchar_t **argv)
{
    HMODULE module;
    DWORD error;
    LONG status = 0;
    PRtlGetLastNtStatus lastNtStatus;
    char exportName[256];
    FARPROC address;

    if (argc != 3 && argc != 4) {
        fwprintf(stderr, L"Usage: java_loader_probe.exe <dll-directory> <full-dll-path> [export-name]\n");
        return 2;
    }

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    if (!SetDllDirectoryW(argv[1])) {
        wprintf(L"SetDllDirectory failed: %lu\n", GetLastError());
        return 3;
    }

    wprintf(L"KexDllLoaded=%d\n", GetModuleHandleW(L"KexDll.dll") != NULL);
    SetLastError(0);
    module = LoadLibraryExW(argv[2], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    error = GetLastError();
    lastNtStatus = (PRtlGetLastNtStatus)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetLastNtStatus");
    if (lastNtStatus) status = lastNtStatus();
    wprintf(L"Dll=%ls Loaded=%d Win32Error=%lu NtStatus=%08lx\n", argv[2], module != NULL, error, status);
    if (module && argc == 4) {
        if (!WideCharToMultiByte(CP_ACP, 0, argv[3], -1, exportName, sizeof(exportName), NULL, NULL)) {
            wprintf(L"ExportNameConversionError=%lu\n", GetLastError());
            FreeLibrary(module);
            return 4;
        }
        SetLastError(0);
        address = GetProcAddress(module, exportName);
        wprintf(L"Export=%ls Resolved=%d Win32Error=%lu\n", argv[3], address != NULL, GetLastError());
        if (!address) {
            FreeLibrary(module);
            return 5;
        }
    }
    if (module) FreeLibrary(module);
    return module ? 0 : 1;
}
