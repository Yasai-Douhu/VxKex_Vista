#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <stdio.h>
VOID __cdecl mainCRTStartup(VOID)
{
    OSVERSIONINFOEXW version = {sizeof(version)};
    RTL_OSVERSIONINFOEXW native = {sizeof(native)};
    BOOL loaded = GetModuleHandle(L"KexDll.dll") != NULL;
    BOOL ok = GetVersionEx((LPOSVERSIONINFOW)&version);
    NTSTATUS status = RtlGetVersion(&native);
    int count; PWSTR *args = CommandLineToArgvW(GetCommandLine(), &count); FILE *report; BOOL disabled = FALSE, unspoofed = FALSE;
    if (!args) ExitProcess(2);
    if (count != 1) {
        if ((count != 3 && count != 4) || wcscmp(args[1], L"--report")) ExitProcess(87);
        if (count == 4) {
            if (!wcscmp(args[3], L"--disabled")) disabled = TRUE;
            else if (!wcscmp(args[3], L"--unspoofed")) unspoofed = TRUE;
            else ExitProcess(87);
        }
        report = _wfopen(args[2], L"wt"); if (!report) ExitProcess(2);
        fprintf(report, "ProcessBits=%u KexDllLoaded=%u\n", (unsigned)(sizeof(PVOID) * 8), loaded);
        fprintf(report, "GetVersionEx=%lu.%lu.%lu RtlGetVersion=%lu.%lu.%lu Status=%08lx\n",
            version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber,
            native.dwMajorVersion, native.dwMinorVersion, native.dwBuildNumber, status);
        fclose(report);
    }
    LocalFree(args);
    printf("ProcessBits=%u KexDllLoaded=%u\n", (unsigned)(sizeof(PVOID) * 8), loaded);
    printf("GetVersionEx=%lu.%lu.%lu RtlGetVersion=%lu.%lu.%lu Status=%08lx\n",
        version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber,
        native.dwMajorVersion, native.dwMinorVersion, native.dwBuildNumber, status);
    fflush(stdout); // This custom CRT entry calls ExitProcess, which does not flush stdio.
    if (disabled) ExitProcess(!loaded && ok && NT_SUCCESS(status) && version.dwMajorVersion == 6 &&
        version.dwMinorVersion == 0 && native.dwMajorVersion == 6 && native.dwMinorVersion == 0 ? 0 : 1);
    if (unspoofed) ExitProcess(loaded && ok && NT_SUCCESS(status) && version.dwMajorVersion == 6 &&
        version.dwMinorVersion == 0 && native.dwMajorVersion == 6 && native.dwMinorVersion == 0 &&
        version.dwBuildNumber == native.dwBuildNumber ? 0 : 1);
    ExitProcess(loaded && ok && NT_SUCCESS(status) && version.dwMajorVersion == 10 &&
        version.dwMinorVersion == 0 && version.dwBuildNumber == 19045 &&
        native.dwMajorVersion == 10 && native.dwMinorVersion == 0 && native.dwBuildNumber == 19045 ? 0 : 1);
}
