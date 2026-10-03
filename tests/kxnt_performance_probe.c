#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef ULONG (WINAPI *PERF_FN)(LARGE_INTEGER*);
typedef ULONG (WINAPI *GET_STATUS)(void);
typedef void (WINAPI *SET_STATUS)(LONG);
static PERF_FN counter,frequency;
static GET_STATUS getStatus;
static SET_STATUS setStatus;
static FILE *out;
static volatile LONG failures;
static void seed_context(void)
{
    setStatus((LONG)0xc000000d);
    SetLastError(0x13579bdf);
}
static ULONG invoke(PERF_FN fn,LARGE_INTEGER *value,ULONG *exception)
{
    ULONG result=0xdeadbeef;
    *exception=0;
    __try {result=fn(value);} __except(EXCEPTION_EXECUTE_HANDLER){*exception=GetExceptionCode();}
    return result;
}
static void check_bad(const char *name,PERF_FN fn,LARGE_INTEGER *value)
{
    ULONG e,r;
    seed_context();r=invoke(fn,value,&e);
    fprintf(out,"Bad=%s Exception=%08lx LastError=%08lx LastStatus=%08lx\n",name,e,GetLastError(),getStatus());
    if(e!=EXCEPTION_ACCESS_VIOLATION || GetLastError()!=0x13579bdf || getStatus()!=0xc000000d)InterlockedIncrement(&failures);
    (void)r;
}
static BOOL valid_call(PERF_FN fn,LARGE_INTEGER *value)
{
    ULONG e,r,error;
    seed_context();r=invoke(fn,value,&e);error=GetLastError();
    if(e || r!=1 || error!=0x13579bdf || getStatus()!=0xc000000d){InterlockedIncrement(&failures);return FALSE;}
    return TRUE;
}
static DWORD WINAPI worker(PVOID context)
{
    unsigned i; LARGE_INTEGER before,actual,after,rate,expected;
    (void)context;
    QueryPerformanceFrequency(&expected);
    for(i=0;i<1000;i++) {
        QueryPerformanceCounter(&before);
        if(!valid_call(counter,&actual))return 1;
        QueryPerformanceCounter(&after);
        if(actual.QuadPart<before.QuadPart || actual.QuadPart>after.QuadPart)InterlockedIncrement(&failures);
        if(!valid_call(frequency,&rate) || rate.QuadPart!=expected.QuadPart)InterlockedIncrement(&failures);
    }
    return 0;
}
int main(int argc,char **argv)
{
    HMODULE module; BYTE storage[32],*pages; SYSTEM_INFO system;
    HANDLE threads[4]; LARGE_INTEGER expected,start,end,rate; DWORD old,exitCode;
    unsigned i,j; ULONG ex,returned,queryError,queryStatus;
    BOOL win32;
    if(argc<3 || argc>4)return 2;
    out=fopen(argv[2],"w");if(!out)return 3;
    module=LoadLibraryA(argv[1]);if(!module){fprintf(out,"LoadError=%lu\n",GetLastError());return 4;}
    win32=argc==4 && strcmp(argv[3],"Win32")==0;
    counter=(PERF_FN)GetProcAddress(module,win32?"QueryPerformanceCounter":"RtlQueryPerformanceCounter");
    frequency=(PERF_FN)GetProcAddress(module,win32?"QueryPerformanceFrequency":"RtlQueryPerformanceFrequency");
    getStatus=(GET_STATUS)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetLastNtStatus");
    setStatus=(SET_STATUS)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    if(!getStatus || !setStatus)return 9;
    if(!counter || !frequency){fprintf(out,"MissingExports Counter=%d Frequency=%d\n",counter!=NULL,frequency!=NULL);fclose(out);return 5;}
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    QueryPerformanceFrequency(&expected);
    fprintf(out,"Diagnostic NativeFrequency=%I64d\n",expected.QuadPart);
    for(j=0;j<2;j++)for(i=0;i<8;i++) {
        LARGE_INTEGER actual,before,after; unsigned k; BOOL unchanged=TRUE;
        PERF_FN fn=j?frequency:counter;
        memset(storage,0xa5,sizeof(storage));QueryPerformanceCounter(&before);
        seed_context();returned=invoke(fn,(LARGE_INTEGER*)(storage+8+i),&ex);
        queryError=GetLastError();
        queryStatus=getStatus();
        if(ex || returned!=1 || queryError!=0x13579bdf || queryStatus!=0xc000000d)InterlockedIncrement(&failures);
        QueryPerformanceCounter(&after);memcpy(&actual,storage+8+i,8);
        for(k=0;k<sizeof(storage);k++)if((k<8+i || k>=16+i) && storage[k]!=0xa5)unchanged=FALSE;
        fprintf(out,"Diagnostic Unaligned=%s Offset=%u Return=%08lx Exception=%08lx LastError=%08lx LastStatus=%08lx GuardsUnchanged=%d Value=%I64d\n",j?"frequency":"counter",i,returned,ex,queryError,queryStatus,unchanged,actual.QuadPart);
        if(!unchanged || (j?(actual.QuadPart!=expected.QuadPart):(actual.QuadPart<before.QuadPart || actual.QuadPart>after.QuadPart)))InterlockedIncrement(&failures);
    }
    fprintf(out,"UnalignedOutputCases=16 Failures=%ld\n",failures);
    GetSystemInfo(&system);pages=(BYTE*)VirtualAlloc(NULL,system.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!pages)return 6;
    VirtualProtect(pages+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&old);
    check_bad("counter-null",counter,NULL);check_bad("frequency-null",frequency,NULL);
    check_bad("counter-invalid",counter,(LARGE_INTEGER*)(ULONG_PTR)1);check_bad("frequency-invalid",frequency,(LARGE_INTEGER*)(ULONG_PTR)1);
    check_bad("counter-guard",counter,(LARGE_INTEGER*)(pages+system.dwPageSize));
    check_bad("frequency-guard",frequency,(LARGE_INTEGER*)(pages+system.dwPageSize));
    check_bad("counter-tail-guard",counter,(LARGE_INTEGER*)(pages+system.dwPageSize-4));
    check_bad("frequency-tail-guard",frequency,(LARGE_INTEGER*)(pages+system.dwPageSize-4));
    VirtualProtect(pages,system.dwPageSize,PAGE_READONLY,&old);
    check_bad("counter-readonly",counter,(LARGE_INTEGER*)pages);check_bad("frequency-readonly",frequency,(LARGE_INTEGER*)pages);
    VirtualFree(pages,0,MEM_RELEASE);
    for(i=0;i<4;i++){threads[i]=CreateThread(NULL,0,worker,NULL,0,NULL);if(!threads[i])return 7;}
    if(WaitForMultipleObjects(4,threads,TRUE,30000)!=WAIT_OBJECT_0)return 8;
    for(i=0;i<4;i++){GetExitCodeThread(threads[i],&exitCode);if(exitCode)InterlockedIncrement(&failures);CloseHandle(threads[i]);}
    start.QuadPart=0;end.QuadPart=0;rate.QuadPart=0;
    if(!valid_call(counter,&start) || !valid_call(frequency,&rate))InterlockedIncrement(&failures);
    Sleep(30);if(!valid_call(counter,&end))InterlockedIncrement(&failures);
    if(rate.QuadPart<=0 || end.QuadPart<=start.QuadPart || (end.QuadPart-start.QuadPart)*1000/rate.QuadPart<10)InterlockedIncrement(&failures);
    fprintf(out,"Diagnostic SleepMilliseconds=%I64d\n",rate.QuadPart>0?(end.QuadPart-start.QuadPart)*1000/rate.QuadPart:0);
    fprintf(out,"Threads=4 CounterCalls=4000 FrequencyCalls=4000 InvalidOutputCases=10 Failures=%ld\n",failures);
    fprintf(out,"Result=%s\n",failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
