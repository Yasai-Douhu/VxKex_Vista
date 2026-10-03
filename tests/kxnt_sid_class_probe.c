#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>

typedef BOOLEAN (WINAPI *CLASS_FN)(PSID);
int main(int argc, char **argv)
{
    HMODULE module;
    CLASS_FN query;
    FILE *out;
    unsigned revision, count, authority, rid, cases = 0;
    const BYTE counts[] = {0,1,2,3,7,8,9,12,15,16,255};
    struct { BYTE revision, count; BYTE authority[6]; DWORD rid[16]; } sid;
    if (argc != 4) return 2;
    out = fopen(argv[2], "w");
    if (!out) return 3;
    fprintf(out, "ProcessBits=%u\n", (unsigned)(sizeof(void *) * 8));
    module = LoadLibraryA(argv[1]);
    if (!module) { fprintf(out,"LoadLibraryError=%lu\n",GetLastError()); fclose(out); return 4; }
    query = (CLASS_FN)GetProcAddress(module, argv[3]);
    if (!query) { fprintf(out,"ExportMissing\n"); fclose(out); return 5; }
    for (revision = 0; revision < 3; ++revision)
    for (count = 0; count < sizeof(counts); ++count)
    for (authority = 0; authority < 4; ++authority)
    for (rid = 0; rid < 5; ++rid) {
        BOOLEAN value = FALSE;
        DWORD exception = 0;
        ZeroMemory(&sid, sizeof(sid));
        sid.revision = (BYTE)revision;
        sid.count = counts[count];
        sid.authority[5] = authority == 0 ? 15 : authority == 1 ? 5 : authority == 2 ? 0 : 15;
        if (authority == 3) sid.authority[0] = 1;
        sid.rid[0] = rid;
        sid.rid[1] = 1;
        __try { value = query((PSID)&sid); }
        __except(EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
        fprintf(out,"Rev=%u Count=%u Auth=%u Rid=%u Value=%u Exception=%08lx\n",
            revision, counts[count], authority, rid, (unsigned)value, exception);
        ++cases;
    }
    {
        DWORD exception = 0;
        BOOLEAN value = FALSE;
        __try { value = query(NULL); }
        __except(EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
        fprintf(out,"NULL Value=%u Exception=%08lx\n",(unsigned)value, exception);
    }
    {
        SYSTEM_INFO system;
        BYTE *pages;
        DWORD old;
        unsigned bytes;
        GetSystemInfo(&system);
        pages = (BYTE *)VirtualAlloc(NULL, system.dwPageSize * 2,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!pages) { fclose(out); return 6; }
        if (!VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &old)) {
            VirtualFree(pages,0,MEM_RELEASE); fclose(out); return 7;
        }
        ZeroMemory(&sid,sizeof(sid));
        sid.revision = 1; sid.count = 2; sid.authority[5] = 15; sid.rid[0] = 2;
        for (bytes = 0; bytes <= 16; ++bytes) {
            BYTE *input = pages + system.dwPageSize - bytes;
            DWORD exception = 0;
            BOOLEAN value = FALSE;
            memcpy(input,&sid,bytes);
            __try { value = query(input); }
            __except(EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
            fprintf(out,"GuardBytes=%u Value=%u Exception=%08lx\n",bytes,(unsigned)value,exception);
        }
        VirtualFree(pages,0,MEM_RELEASE);
    }
    fprintf(out,"SidClassificationCases=%u\nResult=PASS\n",cases);
    fclose(out);
    return 0;
}
