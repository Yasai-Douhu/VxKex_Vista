#include "../VxlView/buildcfg.h"
#include <KexComm.h>
#include <KexDll.h>
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define OUTPUT "C:\\VxKexProbe\\NextParity\\vxl-fixture-result-x64.txt"
#else
#define SUFFIX L"x86"
#define OUTPUT "C:\\VxKexProbe\\NextParity\\vxl-fixture-result-x86.txt"
#endif
VOID __cdecl mainCRTStartup(VOID)
{
    const WCHAR *name = L"\\??\\C:\\VxKexProbe\\NextParity\\viewer-fixture-" SUFFIX L".vxl";
    UNICODE_STRING fileName, application;
    OBJECT_ATTRIBUTES attributes;
    VXLHANDLE handle = NULL;
    VXLLOGENTRY entry;
    NTSTATUS status;
    ULONG index, count = 0, size = sizeof(count);
    int failures = 0;
    FILE *log = fopen(OUTPUT, "wt");
    if (!log) ExitProcess(2);
    setbuf(log, NULL);
    RtlInitUnicodeString(&fileName, name);
    RtlInitUnicodeString(&application, L"VxlFixtureProbe");
    InitializeObjectAttributes(&attributes, &fileName, OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = VxlOpenLog(&handle, &application, &attributes, GENERIC_WRITE, FILE_CREATE);
    fprintf(log, "Create=%08lx\n", status);
    if (!NT_SUCCESS(status)) { failures = 1; goto Done; }
    for (index = 0; index < LogSeverityMaximumValue; ++index) {
        status = VxlWriteLogEx(handle, L"Fixture", L"fixture.c", 100 + index,
            L"WriteFixture", (VXLSEVERITY)index,
            L"%s record %lu\r\n\r\nUnicode body: \x65e5\x672c\x8a9e %lu",
            index == LogSeverityWarning ? L"Warning Alpha" : L"Other Beta", index, index);
        fprintf(log, "Write[%lu]=%08lx\n", index, status);
        if (!NT_SUCCESS(status)) ++failures;
    }
    status = VxlCloseLog(&handle);
    fprintf(log, "CloseWriter=%08lx\n", status);
    if (!NT_SUCCESS(status)) ++failures;
    status = VxlOpenLog(&handle, NULL, &attributes, GENERIC_READ, FILE_OPEN);
    fprintf(log, "OpenReader=%08lx\n", status);
    if (!NT_SUCCESS(status)) { ++failures; goto Done; }
    status = VxlQueryInformationLog(handle, LogTotalNumberOfEvents, &count, &size);
    fprintf(log, "Query=%08lx Count=%lu\n", status, count);
    if (!NT_SUCCESS(status) || count != LogSeverityMaximumValue) ++failures;
    for (index = 0; index < count && index < LogSeverityMaximumValue; ++index) {
        status = VxlReadLog(handle, index, &entry);
        fprintf(log, "Read[%lu]=%08lx severity=%d line=%lu\n", index, status,
            NT_SUCCESS(status) ? entry.Severity : -1,
            NT_SUCCESS(status) ? entry.SourceLine : 0);
        if (!NT_SUCCESS(status) || entry.Severity != index || entry.SourceLine != 100 + index ||
            !entry.Text.Buffer || !wcsstr(entry.Text.Buffer, L"\x65e5\x672c\x8a9e")) ++failures;
    }
Done:
    if (handle) VxlCloseLog(&handle);
    fprintf(log, "Failures=%d\n", failures);
    fclose(log);
    ExitProcess(failures ? 1 : 0);
}
