#include "../VxlView/buildcfg.h"
#include <KexComm.h>
#include <KexDll.h>
#include <stdio.h>
#ifdef _WIN64
#define SUFFIX L"x64"
#define OUTPUT "C:\\VxKexProbe\\NextParity\\vxl-reader-result-x64.txt"
#else
#define SUFFIX L"x86"
#define OUTPUT "C:\\VxKexProbe\\NextParity\\vxl-reader-result-x86.txt"
#endif
static int malformed(FILE *log)
{
    HANDLE file; DWORD size, bytes, test; BYTE *original, *changed;
    WCHAR path[MAX_PATH]; UNICODE_STRING ntPath; OBJECT_ATTRIBUTES attributes;
    VXLHANDLE handle; NTSTATUS status; int failures = 0;
    file = CreateFile(L"C:\\VxKexProbe\\NextParity\\viewer-reader-fixture-" SUFFIX L".vxl", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return 1;
    size = GetFileSize(file, NULL); original = SafeAlloc(BYTE, size); changed = SafeAlloc(BYTE, size);
    if (!ReadFile(file, original, size, &bytes, NULL) || bytes != size) { CloseHandle(file); SafeFree(original); SafeFree(changed); return 1; }
    CloseHandle(file);
    for (test=0; test<10; ++test) {
        PVXLLOGFILEHEADER header = (PVXLLOGFILEHEADER)changed;
        PVXLLOGFILEENTRY entry = (PVXLLOGFILEENTRY)(changed + sizeof(VXLLOGFILEHEADER));
        DWORD writtenSize=size;
        CopyMemory(changed, original, size);
        switch (test) {
        case 0: writtenSize=sizeof(VXLLOGFILEHEADER)-1; break;
        case 1: writtenSize=size-1; break;
        case 2: header->EventSeverityTypeCount[0]=0xffffffffUL; break;
        case 3: entry->Severity=LogSeverityMaximumValue; break;
        case 4: entry->SourceComponentIndex=64; break;
        case 5: entry->SourceFileIndex=128; break;
        case 6: entry->Text[entry->TextHeaderCch-1]=L'X'; break;
        case 7: entry->TextCch=0xffff; break;
        case 8: FillMemory(header->SourceApplication, sizeof(header->SourceApplication), 'X'); break;
        case 9: FillMemory(header->SourceFunctions[0], sizeof(header->SourceFunctions[0]), 'X'); break;
        }
        StringCchPrintf(path, ARRAYSIZE(path), L"C:\\VxKexProbe\\NextParity\\reader-malformed-" SUFFIX L"-%lu.vxl", test);
        file=CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) { ++failures; continue; }
        if (!WriteFile(file, changed, writtenSize, &bytes, NULL) || bytes != writtenSize) ++failures;
        CloseHandle(file);
        status=RtlDosPathNameToNtPathName_U_WithStatus(path, &ntPath, NULL, NULL);
        if (!NT_SUCCESS(status)) { ++failures; DeleteFile(path); continue; }
        InitializeObjectAttributes(&attributes, &ntPath, OBJ_CASE_INSENSITIVE, NULL, NULL); handle=NULL;
        status=VxlOpenLog(&handle, NULL, &attributes, GENERIC_READ, FILE_OPEN);
        fprintf(log, "Malformed[%lu]=%08lx Handle=%p\n", test, status, handle);
        if (NT_SUCCESS(status) || handle) ++failures;
        if (handle) VxlCloseLog(&handle);
        RtlFreeUnicodeString(&ntPath);
        if (!DeleteFile(path)) ++failures;
    }
    SafeFree(original); SafeFree(changed); return failures;
}
VOID __cdecl mainCRTStartup(VOID)
{
    const WCHAR *name = L"\\??\\C:\\VxKexProbe\\NextParity\\viewer-reader-fixture-" SUFFIX L".vxl";
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
    {
        PVXLLOGENTRY entries[1] = {&entry};
        status = VxlReadMultipleEntriesLog(handle, 0, 0, entries);
        fprintf(log, "RejectWriterRead=%08lx\n", status);
        if (status != STATUS_INVALID_OPEN_MODE) ++failures;
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
    status = VxlReadLog(handle, count, &entry);
    fprintf(log, "RejectOnePastEnd=%08lx\n", status);
    if (status != STATUS_NO_MORE_ENTRIES) ++failures;
    {
        VXLLOGENTRY items[LogSeverityMaximumValue];
        PVXLLOGENTRY entries[LogSeverityMaximumValue];
        ZeroMemory(items, sizeof(items));
        for (index=0; index<LogSeverityMaximumValue; ++index) entries[index]=&items[index];
        status = VxlReadMultipleEntriesLog(handle, 0, count+10, entries);
        fprintf(log, "ReadInclusiveClippedRange=%08lx\n", status);
        if (!NT_SUCCESS(status)) ++failures;
        for (index=0; index<count; ++index)
            if (items[index].Severity != index || items[index].SourceLine != 100+index) ++failures;
        ZeroMemory(items, sizeof(items));
        status = VxlReadMultipleEntriesLog(handle, count-1, count-1, entries);
        fprintf(log, "ReadSingleLastRange=%08lx\n", status);
        if (!NT_SUCCESS(status) || items[0].SourceLine != 100+count-1) ++failures;
        entries[0]=NULL;
        status = VxlReadMultipleEntriesLog(handle, 0, 0, entries);
        if (status != STATUS_INVALID_PARAMETER) ++failures;
    }
Done:
    if (handle) VxlCloseLog(&handle);
    if (!failures) failures += malformed(log);
    fprintf(log, "Failures=%d\n", failures);
    fclose(log);
    ExitProcess(failures ? 1 : 0);
}
