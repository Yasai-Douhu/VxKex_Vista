#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *REPORT_FN)(HANDLE,LONG);
static FILE *out;
static REPORT_FN report;
static unsigned failures;
static void call(const char *name,HANDLE handle,LONG expected)
{
    LONG status=(LONG)0xdeadbeef; ULONG exception=0; DWORD error;
    SetLastError(0x13579bdf);
    __try {status=report(handle,42);} __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    error=GetLastError();
    fprintf(out,"Case=%s Status=%08lx Exception=%08lx LastError=%08lx\n",name,(ULONG)status,exception,error);fflush(out);
    if(status!=expected || exception || error!=0x13579bdf)failures++;
}
int main(int argc,char **argv)
{
    HMODULE module; HANDLE handle,closed; LONG valid; DWORD code,before,after; unsigned i;
    if(argc!=3 && argc!=4)return 2;
    valid=argc==4 && strcmp(argv[3],"Vista")==0?(LONG)0xc00000bb:0;
    out=fopen(argv[2],"w");if(!out)return 3;
    module=LoadLibraryA(argv[1]);if(!module){fprintf(out,"LoadError=%lu\n",GetLastError());return 4;}
    report=(REPORT_FN)GetProcAddress(module,"RtlReportSilentProcessExit");
    if(!report){fprintf(out,"MissingExport\n");fclose(out);return 5;}
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    call("null",NULL,(LONG)0xc000000d);call("invalid",(HANDLE)(ULONG_PTR)0x7ffffffc,(LONG)0xc0000008);
    call("self",GetCurrentProcess(),valid);call("thread",GetCurrentThread(),(LONG)0xc000000d);
    handle=CreateEventW(NULL,FALSE,FALSE,NULL);call("event",handle,(LONG)0xc000000d);CloseHandle(handle);
    DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&handle,0,FALSE,DUPLICATE_SAME_ACCESS);
    call("self-real",handle,valid);closed=handle;CloseHandle(handle);call("closed",closed,(LONG)0xc0000008);
    DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&handle,0,FALSE,0);
    call("self-zero-access",handle,valid);CloseHandle(handle);
    DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&handle,PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0);
    call("self-limited-query",handle,valid);CloseHandle(handle);
    if(!GetExitCodeProcess(GetCurrentProcess(),&code) || code!=STILL_ACTIVE)failures++;
    if(valid) {
        Sleep(2000); // Let independent console initialization settle.
        GetProcessHandleCount(GetCurrentProcess(),&before);
        for(i=0;i<1000;i++)if(report(GetCurrentProcess(),42)!=valid)failures++;
        GetProcessHandleCount(GetCurrentProcess(),&after);
        fprintf(out,"RepeatCalls=1000 HandleDelta=%ld\n",(LONG)after-(LONG)before);
        if(after!=before)failures++;
    }
    fprintf(out,"Failures=%u ReportingSupported=%d\n",failures,valid==0);
    fprintf(out,"Result=%s\n",failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
