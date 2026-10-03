#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
typedef BOOLEAN (WINAPI *TRY_LOCK)(PSRWLOCK);
typedef VOID (WINAPI *LOCK_OP)(PSRWLOCK);
typedef LONG (WINAPI *GET_STATUS)(void);
typedef VOID (WINAPI *SET_STATUS)(LONG);
static FILE *out;static unsigned failures;
#ifdef KXNT_IFEO_SRW_RESOURCE_TRACE
#include "kxnt_utf8_trace.h"
#endif
static TRY_LOCK tryE,tryS;static LOCK_OP acquireE,acquireS,releaseE,releaseS;
static GET_STATUS getstatus;static SET_STATUS seedstatus;
static HANDLE watchdogDone;
static DWORD WINAPI watchdog(PVOID ignored) {
    (void)ignored;
    if(WaitForSingleObject(watchdogDone,15000)==WAIT_TIMEOUT){fprintf(out,"Owned SRW probe timed out\n");fflush(out);TerminateProcess(GetCurrentProcess(),9);}
    return 0;
}
static void check(const char *name,BOOL ok){fprintf(out,"Case=%s Pass=%d\n",name,ok);if(!ok)++failures;}
static void attempt(const char *name,TRY_LOCK fn,SRWLOCK *lock,BOOLEAN expected) {
    BOOLEAN result;DWORD start=GetTickCount();
    seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);result=fn(lock);
    check(name,result==expected && GetLastError()==0x13579bdf && getstatus()==(LONG)0xc0000022);
    check("try-does-not-wait",GetTickCount()-start<1000);
    fprintf(out,"Try=%s Result=%u\n",name,result);
}
typedef struct {SRWLOCK *Lock;BOOL Shared;HANDLE Started,Acquired,Go;} WAITER;
static DWORD WINAPI waiter(PVOID argument) {
    WAITER *w=argument;SetEvent(w->Started);
    if(w->Shared)acquireS(w->Lock);else acquireE(w->Lock);
    SetEvent(w->Acquired);
    if(WaitForSingleObject(w->Go,5000)!=WAIT_OBJECT_0)return 1;
    if(w->Shared)releaseS(w->Lock);else releaseE(w->Lock);return 0;
}
static void queued(BOOL sharedOwner,BOOL sharedWaiter,BOOL invoke) {
    SRWLOCK lock=SRWLOCK_INIT;WAITER w;HANDLE thread;DWORD deadline,code;
    ZeroMemory(&w,sizeof(w));w.Lock=&lock;w.Shared=sharedWaiter;
    w.Started=CreateEventW(NULL,TRUE,FALSE,NULL);w.Acquired=CreateEventW(NULL,TRUE,FALSE,NULL);w.Go=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(sharedOwner)acquireS(&lock);else acquireE(&lock);
    thread=CreateThread(NULL,0,waiter,&w,0,NULL);check("waiter-created",thread!=NULL);
    WaitForSingleObject(w.Started,5000);deadline=GetTickCount();
    while(!((ULONG_PTR)lock.Ptr&0xe) && GetTickCount()-deadline<5000)Sleep(1);
    check("native-waiter-enqueued",((ULONG_PTR)lock.Ptr&0xe)!=0 && WaitForSingleObject(w.Acquired,0)==WAIT_TIMEOUT);
    fprintf(out,"Layout OwnerShared=%d WaiterShared=%d WaitBits=%Ix\n",sharedOwner,sharedWaiter,(ULONG_PTR)lock.Ptr&0xf);
    if(invoke){PVOID before=lock.Ptr;attempt("queued-exclusive-fails",tryE,&lock,FALSE);attempt("queued-shared-fails",tryS,&lock,FALSE);check("queued-word-unchanged",lock.Ptr==before);}
    if(sharedOwner)releaseS(&lock);else releaseE(&lock);
    check("native-waiter-acquires",WaitForSingleObject(w.Acquired,5000)==WAIT_OBJECT_0);SetEvent(w.Go);
    if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fprintf(out,"Owned waiter stuck\n");fflush(out);ExitProcess(9);}
    GetExitCodeThread(thread,&code);check("native-waiter-exits",code==0 && lock.Ptr==NULL);
    CloseHandle(thread);CloseHandle(w.Started);CloseHandle(w.Acquired);CloseHandle(w.Go);
}
static void faults(TRY_LOCK fn,const char *name) {
    BYTE *pages=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD old;unsigned i;LONG ex;BOOLEAN result;
    SRWLOCK *ptrs[4];VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&old);
    ptrs[0]=NULL;ptrs[1]=(SRWLOCK*)(ULONG_PTR)1;ptrs[2]=(SRWLOCK*)(pages+4096);ptrs[3]=(SRWLOCK*)(pages+4096-sizeof(SRWLOCK)/2);
    for(i=0;i<4;++i){ex=0;result=127;seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);__try{result=fn(ptrs[i]);}__except(EXCEPTION_EXECUTE_HANDLER){ex=GetExceptionCode();}check("invalid-pointer-native-fault",ex==(LONG)0xc0000005 && result==127 && GetLastError()==0x13579bdf && getstatus()==(LONG)0xc0000022);fprintf(out,"Fault=%s Index=%u Exception=%08lx\n",name,i,(unsigned long)ex);}
    VirtualFree(pages,0,MEM_RELEASE);
}
static void readonly(TRY_LOCK fn,const char *name) {
    BYTE *page=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);SRWLOCK *lock=(SRWLOCK*)page;
    unsigned state;DWORD old;LONG ex;BOOLEAN result;
    for(state=0;state<3;++state){
        ZeroMemory(page,4096);if(state==1)acquireE(lock);else if(state==2)acquireS(lock);
        VirtualProtect(page,4096,PAGE_READONLY,&old);ex=0;result=127;
        seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);
        __try{result=fn(lock);}__except(EXCEPTION_EXECUTE_HANDLER){ex=GetExceptionCode();}
        check("readonly-native-behavior",ex==(LONG)0xc0000005 && result==127 && GetLastError()==0x13579bdf && getstatus()==(LONG)0xc0000022);
        fprintf(out,"Readonly=%s State=%u Result=%u Exception=%08lx\n",name,state,result,(unsigned long)ex);
        VirtualProtect(page,4096,PAGE_READWRITE,&old);if(state==1)releaseE(lock);else if(state==2)releaseS(lock);
    }
    VirtualFree(page,0,MEM_RELEASE);
}
static SRWLOCK mixedLock=SRWLOCK_INIT;
static volatile LONG readers,writers;static ULONG sequence,complement=~0UL;
static DWORD WINAPI mixedWorker(PVOID argument) {
    unsigned mode=(unsigned)(ULONG_PTR)argument,i;DWORD errors=0,start;BOOL held;
    for(i=0;i<1000;++i){
        if(i&1){if(mode&1)acquireS(&mixedLock);else acquireE(&mixedLock);}
        else{start=GetTickCount();do{held=(mode&1?tryS:tryE)(&mixedLock);if(!held)Sleep(0);}while(!held && GetTickCount()-start<5000);if(!held)return 1;}
        if(mode&1){InterlockedIncrement(&readers);if(writers || complement!=~sequence)++errors;InterlockedDecrement(&readers);releaseS(&mixedLock);}
        else{if(InterlockedIncrement(&writers)!=1 || readers)++errors;++sequence;complement=~sequence;InterlockedDecrement(&writers);releaseE(&mixedLock);}
    }
    return errors;
}
static void mixed(void) {
    HANDLE threads[4];DWORD wait,errors,before,after;unsigned i;
#ifdef KXNT_IFEO_SRW_RESOURCE_TRACE
    utf8_snapshot("srw-before");
#endif
#ifdef KXNT_IFEO_RESOURCE_PHASES
    KxNtIfeoResourcePhase(0);
#endif
    GetProcessHandleCount(GetCurrentProcess(),&before);
    for(i=0;i<4;++i)threads[i]=CreateThread(NULL,0,mixedWorker,(PVOID)(ULONG_PTR)i,0,NULL);
    wait=WaitForMultipleObjects(4,threads,TRUE,10000);check("mixed-workers-finish",wait==WAIT_OBJECT_0);
    if(wait!=WAIT_OBJECT_0){fflush(out);TerminateProcess(GetCurrentProcess(),9);}
    for(i=0;i<4;++i){GetExitCodeThread(threads[i],&errors);check("mixed-worker-errors",errors==0);CloseHandle(threads[i]);}
    GetProcessHandleCount(GetCurrentProcess(),&after);
#ifdef KXNT_IFEO_RESOURCE_PHASES
    KxNtIfeoResourcePhase(1);
#endif
    check("mixed-protected-data",sequence==2000 && complement==~sequence && readers==0 && writers==0 && mixedLock.Ptr==NULL);
    check("mixed-handle-delta",after==before);
    fprintf(out,"MixedOperations=4000 Threads=4 Writes=%lu HandleDelta=%ld\n",sequence,(LONG)after-(LONG)before);
#ifdef KXNT_IFEO_SRW_RESOURCE_TRACE
    utf8_snapshot("srw-after");
#endif
}
typedef struct {SRWLOCK Lock;CONDITION_VARIABLE Cv;HANDLE Started;BOOL Shared;ULONG Value;} CV_CONTEXT;
static DWORD WINAPI cvWorker(PVOID argument) {
    CV_CONTEXT *c=argument;BOOL ok;
    if(!(c->Shared?tryS:tryE)(&c->Lock))return 1;
    SetEvent(c->Started);
    ok=SleepConditionVariableSRW(&c->Cv,&c->Lock,3000,c->Shared?CONDITION_VARIABLE_LOCKMODE_SHARED:0);
    if(c->Shared)releaseS(&c->Lock);else releaseE(&c->Lock);
    return ok && c->Value==1 ? 0 : 1;
}
static void condition(BOOL shared) {
    CV_CONTEXT c;HANDLE thread;DWORD start,code;BOOLEAN held;
    ZeroMemory(&c,sizeof(c));c.Shared=shared;c.Started=CreateEventW(NULL,TRUE,FALSE,NULL);
    thread=CreateThread(NULL,0,cvWorker,&c,0,NULL);check("cv-worker-created",thread!=NULL);
    WaitForSingleObject(c.Started,3000);start=GetTickCount();
    do{held=tryE(&c.Lock);if(!held)Sleep(0);}while(!held && GetTickCount()-start<3000);
    check("cv-native-releases-try-lock",held!=FALSE);
    if(held){c.Value=1;WakeConditionVariable(&c.Cv);releaseE(&c.Lock);}
    if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0){fflush(out);TerminateProcess(GetCurrentProcess(),9);}
    GetExitCodeThread(thread,&code);check("cv-native-reacquires",code==0 && c.Lock.Ptr==NULL);
    fprintf(out,"ConditionShared=%d Exit=%lu\n",shared,code);CloseHandle(thread);CloseHandle(c.Started);
}
int main(int argc,char **argv) {
    HMODULE native=GetModuleHandleW(L"ntdll.dll"),module;SRWLOCK lock=SRWLOCK_INIT;char path[MAX_PATH];BOOL present;HANDLE watchdogThread;
    if(argc!=3)return 2;out=fopen(argv[2],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    watchdogDone=CreateEventW(NULL,TRUE,FALSE,NULL);watchdogThread=CreateThread(NULL,0,watchdog,NULL,0,NULL);
    acquireE=(LOCK_OP)GetProcAddress(native,"RtlAcquireSRWLockExclusive");acquireS=(LOCK_OP)GetProcAddress(native,"RtlAcquireSRWLockShared");releaseE=(LOCK_OP)GetProcAddress(native,"RtlReleaseSRWLockExclusive");releaseS=(LOCK_OP)GetProcAddress(native,"RtlReleaseSRWLockShared");
    getstatus=(GET_STATUS)GetProcAddress(native,"RtlGetLastNtStatus");seedstatus=(SET_STATUS)GetProcAddress(native,"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    if(!acquireE||!acquireS||!releaseE||!releaseS||!getstatus||!seedstatus)return 4;
    fprintf(out,"ProcessBits=%u KexDllLoaded=%d\n",(unsigned)(sizeof(void*)*8),GetModuleHandleW(L"KexDll.dll")!=NULL);
    acquireS(&lock);fprintf(out,"Layout Shared1=%Ix\n",(ULONG_PTR)lock.Ptr);acquireS(&lock);fprintf(out,"Layout Shared2=%Ix\n",(ULONG_PTR)lock.Ptr);releaseS(&lock);releaseS(&lock);check("native-shared-release",lock.Ptr==NULL);
    acquireE(&lock);fprintf(out,"Layout Exclusive=%Ix\n",(ULONG_PTR)lock.Ptr);releaseE(&lock);check("native-exclusive-release",lock.Ptr==NULL);
    module=!strcmp(argv[1],"native")?native:LoadLibraryA(argv[1]);if(!module)return 6;
    GetModuleFileNameA(module,path,sizeof(path));fprintf(out,"ProviderPath=%s\n",path);
    if(GetModuleHandleW(L"KexDll.dll")){GetModuleFileNameA(GetModuleHandleW(L"KexDll.dll"),path,sizeof(path));fprintf(out,"KexDllPath=%s\n",path);}
    tryE=(TRY_LOCK)GetProcAddress(module,"RtlTryAcquireSRWLockExclusive");tryS=(TRY_LOCK)GetProcAddress(module,"RtlTryAcquireSRWLockShared");present=tryE && tryS;
    fprintf(out,"TryExportsPresent=%d\n",present);
    queued(TRUE,FALSE,present);queued(FALSE,TRUE,present);queued(FALSE,FALSE,present);
    if(!present){fprintf(out,"Failures=%u Result=NATIVE_TRY_ABSENT\n",failures);fclose(out);return failures?1:5;}
    attempt("exclusive-empty",tryE,&lock,TRUE);attempt("exclusive-held",tryE,&lock,FALSE);attempt("shared-under-exclusive",tryS,&lock,FALSE);releaseE(&lock);check("release-try-exclusive",lock.Ptr==NULL);
    attempt("shared-empty",tryS,&lock,TRUE);attempt("shared-again",tryS,&lock,TRUE);attempt("exclusive-under-shared",tryE,&lock,FALSE);releaseS(&lock);releaseS(&lock);check("release-try-shared",lock.Ptr==NULL);
    acquireS(&lock);attempt("shared-mixed-native",tryS,&lock,TRUE);releaseS(&lock);releaseS(&lock);check("mixed-release-empty",lock.Ptr==NULL);
    faults(tryE,"exclusive");faults(tryS,"shared");
    readonly(tryE,"exclusive");readonly(tryS,"shared");
    condition(FALSE);condition(TRUE);mixed();
    SetEvent(watchdogDone);WaitForSingleObject(watchdogThread,1000);CloseHandle(watchdogThread);CloseHandle(watchdogDone);
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
