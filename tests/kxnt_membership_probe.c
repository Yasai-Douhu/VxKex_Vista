#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *MEMBERSHIP_FN)(HANDLE, PSID, ULONG, PBOOLEAN);
static FILE *out;
static MEMBERSHIP_FN query;
static unsigned cases, failures;

static void test(const char *name, HANDLE token, PSID sid, ULONG flags, int nullOutput)
{
    struct { BYTE before; BOOLEAN member; BYTE after; } result = {0x5a,0x7f,0xa5};
    LONG status = 0;
    DWORD exception = 0, lastError;
    SetLastError(0x12345678);
    __try { status = query(token,sid,flags,nullOutput ? NULL : &result.member); }
    __except(EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
    lastError = GetLastError();
    fprintf(out,"Case=%s Flags=%08lx Status=%08lx Member=%u Exception=%08lx Guard=%u LastError=%08lx\n",
        name,flags,(ULONG)status,(unsigned)result.member,exception,
        result.before == 0x5a && result.after == 0xa5,lastError);
    if (result.before != 0x5a || result.after != 0xa5 || lastError != 0x12345678) ++failures;
    ++cases;
}

int main(int argc, char **argv)
{
    HMODULE module;
    HANDLE primary, impersonation, restricted, denyOnly, queryOnly, anonymous, wrongType, noQuery;
    SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    PSID world, system;
    SID_AND_ATTRIBUTES restrictSid, disableSid;
    BYTE userBuffer[512], badSid[16];
    DWORD size;
    unsigned i;
    const ULONG flags[] = {0,1,2,3,4,0xffffffff};
    if (argc != 3) return 2;
    out = fopen(argv[2],"w");
    if (!out) return 3;
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    module = LoadLibraryA(argv[1]);
    if (!module) return 4;
    query = (MEMBERSHIP_FN)GetProcAddress(module,"RtlCheckTokenMembershipEx");
    if (!query) { fprintf(out,"ExportMissing\n"); fclose(out); return 5; }
    if (!AllocateAndInitializeSid(&worldAuth,1,0,0,0,0,0,0,0,0,&world) ||
        !AllocateAndInitializeSid(&ntAuth,1,SECURITY_LOCAL_SYSTEM_RID,0,0,0,0,0,0,0,&system) ||
        !OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY,&primary) ||
        !DuplicateTokenEx(primary,TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE,NULL,SecurityImpersonation,TokenImpersonation,&impersonation) ||
        !DuplicateTokenEx(primary,TOKEN_QUERY,NULL,SecurityImpersonation,TokenImpersonation,&queryOnly) ||
        !DuplicateTokenEx(primary,TOKEN_QUERY,NULL,SecurityAnonymous,TokenImpersonation,&anonymous) ||
        !DuplicateTokenEx(primary,TOKEN_DUPLICATE,NULL,SecurityImpersonation,TokenImpersonation,&noQuery) ||
        !GetTokenInformation(primary,TokenUser,userBuffer,sizeof(userBuffer),&size)) {
        fprintf(out,"SetupError=%lu\n",GetLastError()); fclose(out); return 6;
    }
    restrictSid.Sid=world; restrictSid.Attributes=0;
    disableSid.Sid=world; disableSid.Attributes=0;
    if (!CreateRestrictedToken(impersonation,0,0,NULL,0,NULL,1,&restrictSid,&restricted) ||
        !CreateRestrictedToken(impersonation,0,1,&disableSid,0,NULL,0,NULL,&denyOnly)) {
        fprintf(out,"RestrictSetupError=%lu\n",GetLastError()); fclose(out); return 7;
    }
    wrongType=CreateEvent(NULL,FALSE,FALSE,NULL);
    ZeroMemory(badSid,sizeof(badSid));
    for (i=0; i<sizeof(flags)/sizeof(flags[0]); ++i) {
        test("null-world",NULL,world,flags[i],0);
        test("null-system",NULL,system,flags[i],0);
        test("imp-world",impersonation,world,flags[i],0);
        test("imp-user",impersonation,((TOKEN_USER*)userBuffer)->User.Sid,flags[i],0);
        test("primary",primary,world,flags[i],0);
        test("invalid-handle",(HANDLE)(ULONG_PTR)0x123456,world,flags[i],0);
        test("wrong-type",wrongType,world,flags[i],0);
        test("query-only",queryOnly,world,flags[i],0);
        test("no-query",noQuery,world,flags[i],0);
        test("anonymous",anonymous,world,flags[i],0);
        test("restricted-world",restricted,world,flags[i],0);
        test("restricted-user",restricted,((TOKEN_USER*)userBuffer)->User.Sid,flags[i],0);
        test("deny-only",denyOnly,world,flags[i],0);
        test("invalid-sid",impersonation,badSid,flags[i],0);
        test("null-sid",impersonation,NULL,flags[i],0);
        test("null-output",impersonation,world,flags[i],1);
    }
    if (!SetThreadToken(NULL,restricted)) return 8;
    test("thread-restricted-world",NULL,world,0,0);
    test("thread-restricted-user",NULL,((TOKEN_USER*)userBuffer)->User.Sid,0,0);
    if (!SetThreadToken(NULL,NULL)) return 9;
    ZeroMemory(badSid,sizeof(badSid));
    badSid[0]=1; badSid[1]=16;
    test("excess-subauthorities",impersonation,badSid,0,0);
    badSid[0]=1; badSid[1]=0;
    test("zero-subauthorities",impersonation,badSid,0,0);
    test("invalid-sid-invalid-handle",(HANDLE)(ULONG_PTR)0x123456,badSid,0,0);
    {
        SYSTEM_INFO info;
        BYTE *pages;
        DWORD old;
        unsigned bytes;
        GetSystemInfo(&info);
        pages=(BYTE*)VirtualAlloc(NULL,info.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if (!pages || !VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old)) return 10;
        for (bytes=0; bytes<=12; ++bytes) {
            char name[32];
            BYTE *input=pages+info.dwPageSize-bytes;
            memcpy(input,world,bytes);
            sprintf(name,"guard-sid-%u",bytes);
            test(name,impersonation,input,0,0);
        }
        VirtualFree(pages,0,MEM_RELEASE);
    }
    {
        DWORD before,after;
        unsigned round;
        if (!GetProcessHandleCount(GetCurrentProcess(),&before)) return 11;
        for (round=0; round<2000; ++round) {
            BOOLEAN member=FALSE;
            if (query(NULL,world,0,&member) != 0 || !member) ++failures;
        }
        if (!GetProcessHandleCount(GetCurrentProcess(),&after)) return 12;
        if (before != after) ++failures;
        fprintf(out,"StressIterations=2000 HandleDelta=%ld\n",(LONG)(after-before));
    }
    CloseHandle(primary); CloseHandle(impersonation); CloseHandle(queryOnly);
    CloseHandle(anonymous); CloseHandle(restricted); CloseHandle(denyOnly); CloseHandle(wrongType);
    CloseHandle(noQuery);
    FreeSid(world); FreeSid(system);
    fprintf(out,"MembershipCases=%u Failures=%u\nResult=%s\n",cases,failures,failures ? "FAIL" : "PASS");
    fclose(out);
    return failures ? 1 : 0;
}
