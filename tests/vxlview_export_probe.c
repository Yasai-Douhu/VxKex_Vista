#include "../VxlView/vxlview.h"
#include "../VxlView/backendp.h"
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\viewer-export-result-x64.txt"
#else
#define SUFFIX L"x86"
#define LOGFILE "C:\\VxKexProbe\\NextParity\\viewer-export-result-x86.txt"
#endif
VOID __cdecl ExportProbeEntry(VOID)
{
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES attributes;
    NTSTATUS status;
    ULONG count = 0, size = sizeof(count), index;
    HANDLE locked;
    WCHAR *text;
    DWORD bytes, readBytes;
    FILE *log = fopen(LOGFILE, "wt");
    PCWSTR destination = L"C:\\VxKexProbe\\NextParity\\viewer-export-" SUFFIX L".txt";
    int failures = 0;
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    InitializeBackend();
    RtlInitUnicodeString(&name, L"\\??\\C:\\VxKexProbe\\NextParity\\viewer-fixture-x64.vxl");
    InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = VxlOpenLog(&State->LogHandle, NULL, &attributes, GENERIC_READ, FILE_OPEN);
    fprintf(log, "Open=%08lx\n", status);
    if (!NT_SUCCESS(status)) { ++failures; goto Done; }
    status = VxlQueryInformationLog(State->LogHandle, LogTotalNumberOfEvents, &count, &size);
    if (!NT_SUCCESS(status) || count != 6) { ++failures; goto Done; }
    State->NumberOfLogEntries = count;
    State->LogEntryCache = SafeAlloc(PLOGENTRYCACHEENTRY, count);
    ZeroMemory(State->LogEntryCache, count * sizeof(PLOGENTRYCACHEENTRY));
    status = ExportLogToFile(destination);
    fprintf(log, "Export=%08lx\n", status);
    if (!NT_SUCCESS(status)) { ++failures; goto Done; }
    locked = CreateFile(destination, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (locked == INVALID_HANDLE_VALUE) { ++failures; goto Done; }
    bytes = GetFileSize(locked, NULL);
    text = SafeAlloc(WCHAR, bytes / sizeof(WCHAR) + 1);
    if (!ReadFile(locked, text, bytes, &readBytes, NULL) || readBytes != bytes ||
        bytes < 2 || (bytes & 1) || text[0] != 0xfeff) ++failures;
    text[bytes / sizeof(WCHAR)] = 0;
    for (index = 0; index < count; ++index) {
        WCHAR expected[64];
        WCHAR date[32];
        PLOGENTRYCACHEENTRY entry = GetLogEntryRaw(index);
        if (!entry || !GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE,
            &entry->LogEntry.Time, NULL, date, ARRAYSIZE(date), NULL) ||
            !wcsstr(text, date)) ++failures;
        StringCchPrintf(expected, ARRAYSIZE(expected), L"record %lu", index);
        if (!wcsstr(text, expected)) ++failures;
        StringCchPrintf(expected, ARRAYSIZE(expected), L"\x65e5\x672c\x8a9e %lu", index);
        if (!wcsstr(text, expected)) ++failures;
    }
    fprintf(log, "Bytes=%lu ContentFailures=%d\n", bytes, failures);
    SafeFree(text);
    status = ExportLogToFile(destination);
    fprintf(log, "LockedDestination=%08lx\n", status);
    if (status != STATUS_SHARING_VIOLATION) ++failures;
    CloseHandle(locked);
    status = ExportLogToFile(L"C:\\VxKexProbe\\NextParity\\NotARealExportDirectory-20261001\\export.txt");
    fprintf(log, "MissingDirectory=%08lx\n", status);
    if (NT_SUCCESS(status)) ++failures;
Done:
    CleanupBackend();
    fprintf(log, "Failures=%d\n", failures);
    fclose(log);
    ExitProcess(failures ? 1 : 0);
}
