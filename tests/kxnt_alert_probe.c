#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LONG (WINAPI *ALERT_FN)(HANDLE);
typedef LONG (WINAPI *WAIT_FN)(PVOID,LARGE_INTEGER*);
static ALERT_FN alertThread;
static WAIT_FN waitAlert;
static FILE *out;
static unsigned failures;
static const DWORD marker=0x543210ab;
static volatile LONG apcCalls;
#define CHURN_THREADS 4096
static char loadedDll[MAX_PATH],outputPath[MAX_PATH];
static void check(const char *name,LONG status,ULONG expected)
{
    int pass=(ULONG)status==expected&&GetLastError()==marker;
    if(!pass)++failures;
    fprintf(out,"%s Status=%08lx LastError=%08lx %s\n",name,(ULONG)status,GetLastError(),pass?"PASS":"FAIL");
}
static LONG alert(HANDLE id)
{SetLastError(marker);return alertThread(id);}
static LONG wait(PVOID hint,LARGE_INTEGER *timeout)
{
    LONG status;SetLastError(marker);
    __try{status=waitAlert(hint,timeout);}
    __except(EXCEPTION_EXECUTE_HANDLER){status=GetExceptionCode();}
    return status;
}
typedef struct {HANDLE ready,gate;LONG first,second;int mode;LONG invalid,alerted,timedout;} ALERT_CONTEXT;
static DWORD WINAPI worker(void *argument)
{
    ALERT_CONTEXT *c=(ALERT_CONTEXT*)argument;LARGE_INTEGER zero;
    if(c->mode==3){
        zero.QuadPart=0;c->first=wait(NULL,&zero);SetEvent(c->ready);
        if(WaitForSingleObject(c->gate,5000)!=WAIT_OBJECT_0)return 2;
        c->second=wait(NULL,&zero);return 0;
    }
    zero.QuadPart=0;SetEvent(c->ready);
    if(WaitForSingleObject(c->gate,5000)!=WAIT_OBJECT_0)return 2;
    if(c->mode==1)return 0;
    if(c->mode==4){c->first=wait(NULL,NULL);return 0;}
    if(c->mode==2){
        LARGE_INTEGER shortWait;unsigned i;shortWait.QuadPart=-10000;
        for(i=0;i<1000;++i){LONG s=wait(NULL,&shortWait);if((ULONG)s==0x101)InterlockedIncrement(&c->alerted);else if((ULONG)s==0x102)InterlockedIncrement(&c->timedout);else InterlockedIncrement(&c->invalid);}
        return 0;
    }
    c->first=wait((void*)(ULONG_PTR)0x123456,&zero);
    c->second=wait(NULL,&zero);
    return 0;
}
static void CALLBACK callback(ULONG_PTR unused)
{(void)unused;InterlockedIncrement(&apcCalls);}
static void currentCases(void)
{
    LARGE_INTEGER zero,relative,absolute;FILETIME now;ULONG_PTR id=GetCurrentThreadId();unsigned i;
    HANDLE ids[5]={NULL,(HANDLE)(ULONG_PTR)0xfffffffc,(HANDLE)(ULONG_PTR)-1,(HANDLE)(ULONG_PTR)GetCurrentProcessId(),(HANDLE)id};
    const char *labels[5]={"alert-null","alert-invalid","alert-minus-one","alert-process-id","alert-self"};
    apcCalls=0;
    zero.QuadPart=0;relative.QuadPart=-10000;
    check("wait-zero",wait(NULL,&zero),0x102);
    check("wait-hint-unreadable",wait((void*)(ULONG_PTR)1,&zero),0x102);
    check("wait-relative",wait(NULL,&relative),0x102);
    GetSystemTimeAsFileTime(&now);absolute.LowPart=now.dwLowDateTime;absolute.HighPart=now.dwHighDateTime;
    absolute.QuadPart-=10000000;
    check("wait-absolute-past",wait(NULL,&absolute),0x102);
    check("wait-timeout-unreadable",wait(NULL,(LARGE_INTEGER*)(ULONG_PTR)1),0xc0000005);
    for(i=0;i<5;++i){
        LONG status=alert(ids[i]);
        check(labels[i],status,i==4?0:0xc000000b);
        if(i==4)check("consume-self",wait(NULL,&zero),0x101);
    }
    for(i=0;i<4;++i){
        LONG status=alert((HANDLE)(id+i));
        check("alert-tagged-self",status,0);
        if(status==0)check("consume-tagged-self",wait(NULL,&zero),0x101);
    }
    for(i=0;i<15;++i)check("alert-self-coalescing",alert((HANDLE)id),0);
    check("coalesced-first",wait(NULL,&zero),0x101);
    check("coalesced-second",wait(NULL,&zero),0x102);
    check("alert-before-invalid-timeout",alert((HANDLE)id),0);
    check("pending-invalid-timeout",wait(NULL,(LARGE_INTEGER*)(ULONG_PTR)1),0xc0000005);
    check("pending-preserved-after-fault",wait(NULL,&zero),0x101);
    if(!QueueUserAPC(callback,GetCurrentThread(),0))++failures;
    check("wait-not-apc-alertable",wait(NULL,&relative),0x102);
    if(apcCalls!=0)++failures;
    SleepEx(0,TRUE);
    fprintf(out,"ApcCallsAfterAlertableWait=%ld\n",apcCalls);
    if(apcCalls!=1)++failures;
}
static void workerCases(void)
{
    ALERT_CONTEXT c;HANDLE thread;DWORD tid,exitCode;unsigned i;
    LARGE_INTEGER zero;zero.QuadPart=0;
    memset(&c,0,sizeof(c));c.ready=CreateEvent(NULL,TRUE,FALSE,NULL);c.gate=CreateEvent(NULL,TRUE,FALSE,NULL);
    thread=CreateThread(NULL,0,worker,&c,0,&tid);
    if(!thread||WaitForSingleObject(c.ready,5000)!=WAIT_OBJECT_0){++failures;return;}
    for(i=0;i<15;++i)check("alert-worker-before-wait",alert((HANDLE)(ULONG_PTR)tid),0);
    SetEvent(c.gate);
    if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fprintf(out,"WorkerTimeout\n");ExitProcess(8);}
    GetExitCodeThread(thread,&exitCode);
    fprintf(out,"WorkerPending First=%08lx Second=%08lx Exit=%lu\n",(ULONG)c.first,(ULONG)c.second,exitCode);
    if((ULONG)c.first!=0x101||(ULONG)c.second!=0x102||exitCode)++failures;
    fprintf(out,"Diagnostic alert-exited-handle-retained=%08lx\n",(ULONG)alert((HANDLE)(ULONG_PTR)tid));
    CloseHandle(thread);
    fprintf(out,"Diagnostic alert-exited-handle-closed=%08lx\n",(ULONG)alert((HANDLE)(ULONG_PTR)tid));
    ResetEvent(c.ready);ResetEvent(c.gate);c.mode=2;c.invalid=0;
    thread=CreateThread(NULL,0,worker,&c,0,&tid);
    if(!thread){++failures;return;}
    SetEvent(c.gate);
    for(i=0;i<1500;++i){LONG s=alert((HANDLE)(ULONG_PTR)tid);if(s!=0&&(ULONG)s!=0xc000000b)++failures;if(i%3==0)Sleep(1);}
    if(WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0){fprintf(out,"RaceTimeout\n");ExitProcess(8);}
    GetExitCodeThread(thread,&exitCode);
    fprintf(out,"RaceWaitCalls=1000 InvalidResults=%ld Exit=%lu\n",c.invalid,exitCode);
    fprintf(out,"Diagnostic RaceAlerted=%ld RaceTimedOut=%ld\n",c.alerted,c.timedout);
    if(c.invalid||exitCode||!c.alerted||!c.timedout)++failures;
    CloseHandle(thread);CloseHandle(c.ready);CloseHandle(c.gate);
    check("current-pending-unaffected",wait(NULL,&zero),0x102);
}
static void foreignCase(void)
{
    WCHAR exe[MAX_PATH],command[MAX_PATH+32];STARTUPINFOW startup;PROCESS_INFORMATION process;LONG status;
    GetModuleFileNameW(NULL,exe,MAX_PATH);swprintf(command,MAX_PATH+32,L"\"%s\" --foreign",exe);
    memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);
    if(!CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_SUSPENDED,NULL,NULL,&startup,&process)){++failures;return;}
    status=alert((HANDLE)(ULONG_PTR)process.dwThreadId);
    check("alert-foreign-thread",status,0xc0000022);
    ResumeThread(process.hThread);
    if(WaitForSingleObject(process.hProcess,5000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,9);++failures;}
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
}
static void lifetimeCases(void)
{
    ALERT_CONTEXT c;DWORD ids[CHURN_THREADS],tid,exitCode,before,after;
    unsigned i,j,reuse=0,earlyErrors=0,laterErrors=0;HANDLE thread;
    LARGE_INTEGER zero;zero.QuadPart=0;
    memset(&c,0,sizeof(c));c.ready=CreateEvent(NULL,TRUE,FALSE,NULL);c.gate=CreateEvent(NULL,TRUE,FALSE,NULL);
    GetProcessHandleCount(GetCurrentProcess(),&before);
    c.mode=1;thread=CreateThread(NULL,0,worker,&c,0,&tid);
    if(!thread||WaitForSingleObject(c.ready,5000)!=WAIT_OBJECT_0){++failures;return;}
    check("alert-exiting-with-pending",alert((HANDLE)(ULONG_PTR)tid),0);
    SetEvent(c.gate);
    if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fprintf(out,"PendingExitTimeout\n");ExitProcess(8);}
    CloseHandle(thread);check("pending-exit-current-unaffected",wait(NULL,&zero),0x102);
    GetProcessHandleCount(GetCurrentProcess(),&after);
    fprintf(out,"PendingExitHandleDelta=%ld\n",(LONG)(after-before));if(before!=after)++failures;
    ResetEvent(c.ready);ResetEvent(c.gate);
    c.mode=4;thread=CreateThread(NULL,0,worker,&c,0,&tid);
    if(!thread||WaitForSingleObject(c.ready,5000)!=WAIT_OBJECT_0){++failures;return;}
    SetEvent(c.gate);Sleep(50);
    check("alert-infinite-wait",alert((HANDLE)(ULONG_PTR)tid),0);
    if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fprintf(out,"InfiniteWaitTimeout\n");ExitProcess(8);}
    fprintf(out,"InfiniteWaitStatus=%08lx\n",(ULONG)c.first);if((ULONG)c.first!=0x101)++failures;
    CloseHandle(thread);wait(NULL,&zero);
    GetProcessHandleCount(GetCurrentProcess(),&before);
    c.mode=3;
    for(i=0;i<CHURN_THREADS;++i){
        ResetEvent(c.ready);ResetEvent(c.gate);
        thread=CreateThread(NULL,0,worker,&c,0,&tid);
        if(!thread||WaitForSingleObject(c.ready,5000)!=WAIT_OBJECT_0){fprintf(out,"ChurnSetupFailure\n");ExitProcess(8);}
        for(j=0;j<i;++j)if(ids[j]==tid){++reuse;break;}ids[i]=tid;
        if((ULONG)c.first!=0x102)++earlyErrors;
        if(alert((HANDLE)(ULONG_PTR)tid)!=0)++laterErrors;
        SetEvent(c.gate);
        if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fprintf(out,"ChurnTimeout\n");ExitProcess(8);}
        GetExitCodeThread(thread,&exitCode);
        if((ULONG)c.second!=0x101||exitCode)++laterErrors;
        CloseHandle(thread);
        // Collect exited compatibility state before allowing ID reuse.
        if((ULONG)wait(NULL,&zero)!=0x102)++laterErrors;
    }
    GetProcessHandleCount(GetCurrentProcess(),&after);
    fprintf(out,"ChurnThreads=%u EarlyErrors=%u LaterErrors=%u HandleDelta=%ld\n",CHURN_THREADS,earlyErrors,laterErrors,(LONG)(after-before));
    fprintf(out,"Diagnostic ReusedThreadIds=%u\n",reuse);
    if(earlyErrors||laterErrors||before!=after)++failures;
    CloseHandle(c.ready);CloseHandle(c.gate);
}
static int terminationChild(const char *dll,const char *log,int alias)
{
    ALERT_CONTEXT c;HMODULE module;HANDLE thread;DWORD tid,before,after;
    LARGE_INTEGER zero;LONG status;int error=0;
    out=fopen(log,"w");if(!out)return 3;
    module=LoadLibraryA(dll);if(!module){fprintf(out,"LoadLibraryError=%lu Path=%s\n",GetLastError(),dll);fclose(out);return 4;}
    // Use the actual provider by full path. Propagation can preload the
    // installed KexDll, which is deliberately not replaced by these tests.
    alertThread=(ALERT_FN)GetProcAddress(module,alias?"ZwAlertThreadByThreadId":"NtAlertThreadByThreadId");
    waitAlert=(WAIT_FN)GetProcAddress(module,alias?"ZwWaitForAlertByThreadId":"NtWaitForAlertByThreadId");
    if(alias&&(!alertThread||!waitAlert)){
        alertThread=(ALERT_FN)GetProcAddress(module,"NtAlertThreadByThreadId");
        waitAlert=(WAIT_FN)GetProcAddress(module,"NtWaitForAlertByThreadId");
    }
    if(!alertThread||!waitAlert){fprintf(out,"ProviderExportsMissing\n");fclose(out);return 6;}
    zero.QuadPart=0;
    if(alert((HANDLE)(ULONG_PTR)GetCurrentThreadId())!=0||(ULONG)wait(NULL,&zero)!=0x101)error=1;
    memset(&c,0,sizeof(c));c.mode=4;
    c.ready=CreateEvent(NULL,TRUE,FALSE,NULL);c.gate=CreateEvent(NULL,TRUE,FALSE,NULL);
    Sleep(2000);GetProcessHandleCount(GetCurrentProcess(),&before);
    thread=CreateThread(NULL,0,worker,&c,0,&tid);
    if(!thread||WaitForSingleObject(c.ready,5000)!=WAIT_OBJECT_0)return 8;
    SetEvent(c.gate);Sleep(100);
    if(!TerminateThread(thread,77)||WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0)error=1;
    CloseHandle(thread);
    status=wait(NULL,&zero);
    if((ULONG)status!=0x102)error=1;
    GetProcessHandleCount(GetCurrentProcess(),&after);
    fprintf(out,"TerminatedWaiter CurrentStatus=%08lx HandleDelta=%ld\n",(ULONG)status,(LONG)(after-before));
    if(before!=after)error=1;
    CloseHandle(c.ready);CloseHandle(c.gate);
    FreeLibrary(module);fprintf(out,"Result=%s\n",error?"FAIL":"PASS");fclose(out);return error?1:0;
}
static void terminationCase(int alias)
{
    char exe[MAX_PATH],command[MAX_PATH*3+128],log[MAX_PATH+32],*line;
    HMODULE provider;
    STARTUPINFOA startup;PROCESS_INFORMATION process;DWORD exitCode;FILE *childLog;char text[512];size_t count;
    GetModuleFileNameA(NULL,exe,MAX_PATH);sprintf(log,"%s-termination-%u.txt",outputPath,alias);
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)alertThread,&provider)||!GetModuleFileNameA(provider,loadedDll,MAX_PATH)){++failures;return;}
    sprintf(command,"\"%s\" --termination \"%s\" \"%s\" %u",exe,loadedDll,log,alias);
    fprintf(out,"Diagnostic TerminationProvider=%s\n",loadedDll);
    memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);
    if(!CreateProcessA(exe,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&process)){++failures;return;}
    // Keep a forced-termination experiment isolated. If a lock was damaged,
    // the parent can end this test process instead of hanging the whole probe.
    if(WaitForSingleObject(process.hProcess,10000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,8);++failures;exitCode=8;}
    else GetExitCodeProcess(process.hProcess,&exitCode);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    childLog=fopen(log,"r");text[0]=0;
    if(childLog){count=fread(text,1,sizeof(text)-1,childLog);text[count]=0;fclose(childLog);}
    fprintf(out,"TerminatedWaiterTest Exit=%lu %s\n",exitCode,exitCode==0&&strstr(text,"Result=PASS")?"PASS":"FAIL");
    if(exitCode||!strstr(text,"Result=PASS"))++failures;
    for(line=strtok(text,"\r\n");line;line=strtok(NULL,"\r\n"))fprintf(out,"Diagnostic TerminationChild %s\n",line);
    if(exitCode==0)DeleteFileA(log);
}
int main(int argc,char **argv)
{
    HMODULE module;int alias,coreOnly;
    if(argc==2&&strcmp(argv[1],"--foreign")==0)return 0;
    if(argc==5&&strcmp(argv[1],"--termination")==0)return terminationChild(argv[2],argv[3],atoi(argv[4]));
    if(argc==4&&strcmp(argv[1],"--provider")==0){
        out=fopen(argv[3],"w");if(!out)return 3;
        module=LoadLibraryA(argv[2]);if(!module)return 4;
        alertThread=(ALERT_FN)GetProcAddress(module,"NtAlertThreadByThreadId");
        if(!alertThread)return 6;
        strncpy(outputPath,argv[3],MAX_PATH-1);terminationCase(0);
        fprintf(out,"Failures=%u\n",failures);fclose(out);FreeLibrary(module);return failures?1:0;
    }
    coreOnly=argc==4&&strcmp(argv[3],"CoreOnly")==0;
    if(argc!=3&&!coreOnly)return 2;
    out=fopen(argv[2],"w");if(!out)return 3;
    module=LoadLibraryA(argv[1]);if(!module)return 4;
    GetModuleFileNameA(module,loadedDll,MAX_PATH);strncpy(outputPath,argv[2],MAX_PATH-1);
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    for(alias=0;alias<2;++alias){
        alertThread=(ALERT_FN)GetProcAddress(module,alias?"ZwAlertThreadByThreadId":"NtAlertThreadByThreadId");
        waitAlert=(WAIT_FN)GetProcAddress(module,alias?"ZwWaitForAlertByThreadId":"NtWaitForAlertByThreadId");
        if(!alertThread||!waitAlert){fprintf(out,"NativeExportsMissing Alert=%u Wait=%u\n",alertThread!=NULL,waitAlert!=NULL);fclose(out);return 6;}
        fprintf(out,"Entry=%s\n",alias?"Zw":"Nt");
        currentCases();workerCases();foreignCase();lifetimeCases();
        if(!coreOnly)terminationCase(alias);
    }
    fprintf(out,"Failures=%u\nResult=%s\n",failures,failures?"FAIL":"PASS");
    fclose(out);FreeLibrary(module);return failures?1:0;
}
