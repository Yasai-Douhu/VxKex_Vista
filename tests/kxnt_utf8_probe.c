#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <intrin.h>
typedef LONG (WINAPI *CONVERT)(PVOID,ULONG,PULONG,const void*,ULONG);
typedef LONG (WINAPI *GET_STATUS)(void);
typedef VOID (WINAPI *SET_STATUS)(LONG);
typedef struct {const char *Name;const void *Data;ULONG Bytes;} UTF_INPUT;
static FILE *out;static GET_STATUS getstatus;static SET_STATUS seedstatus;static unsigned failures;
#ifdef KXNT_UTF8_RESOURCE_TRACE
#include "kxnt_utf8_trace.h"
#endif
static void call(const char *direction,CONVERT fn,const UTF_INPUT *input,ULONG cap,BOOL query,BOOL count) {
    BYTE destination[64];ULONG actual=0xdeadbeef,i;LONG status=0xdeadbeef,exception=0;DWORD error;LONG tls;
    memset(destination,0xa5,sizeof(destination));seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);
    __try {status=fn(query?NULL:destination,cap,count?&actual:NULL,input->Data,input->Bytes);}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    error=GetLastError();tls=getstatus();
    fprintf(out,"Call=%s Input=%s Cap=%lu Query=%d Count=%d Status=%08lx Exception=%08lx Actual=%08lx Error=%08lx TLS=%08lx Data=",direction,input->Name,cap,query,count,status,exception,actual,error,tls);
    for(i=0;i<32;++i)fprintf(out,"%02x",destination[i]);fputc('\n',out);
    if(error!=0x13579bdf || tls!=(LONG)0xc0000022)++failures;
    if(!query && cap<=32)for(i=cap;i<64;++i)if(destination[i]!=0xa5){++failures;break;}
}
static const BYTE ascii[]={0x41,0,0x42};
static const BYTE valid[]={0x41,0xc2,0xa2,0xe6,0x97,0xa5,0xf0,0x9f,0x98,0x80,0};
static const BYTE continuation[]={0x80,0xbf};
static const BYTE truncated2[]={0xc2};
static const BYTE truncated3[]={0xe6,0x97};
static const BYTE truncated4[]={0xf0,0x9f,0x98};
static const BYTE overlong2[]={0xc0,0xaf};
static const BYTE overlong3[]={0xe0,0x80,0xaf};
static const BYTE surrogate[]={0xed,0xa0,0x80};
static const BYTE overmax[]={0xf4,0x90,0x80,0x80};
static const BYTE invalidlead[]={0xf5,0x80,0x80,0x80,0xff};
static const BYTE interrupted[]={0xe2,0x82,0x41,0xc2,0x42};
static const BYTE extremes[]={0,0x7f,0xc2,0x80,0xdf,0xbf,0xe0,0xa0,0x80,0xef,0xbf,0xbf,0xf0,0x90,0x80,0x80,0xf4,0x8f,0xbf,0xbf};
static const WCHAR unicode[]={0x41,0,0x00a2,0x65e5,0xd83d,0xde00,0x42};
static const WCHAR lonehigh[]={0xd800};
static const WCHAR lonelow[]={0xdc00};
static const WCHAR badpair[]={0xd800,0x41,0xdc00,0xd800,0xd800,0xdc00};
static const WCHAR maxunicode[]={0,0x7f,0x80,0x7ff,0x800,0xffff,0xd800,0xdc00,0xdbff,0xdfff};
#define ENTRY(x) {#x,x,sizeof(x)}
static UTF_INPUT utf8[]={ENTRY(ascii),ENTRY(valid),ENTRY(continuation),ENTRY(truncated2),ENTRY(truncated3),ENTRY(truncated4),ENTRY(overlong2),ENTRY(overlong3),ENTRY(surrogate),ENTRY(overmax),ENTRY(invalidlead),ENTRY(interrupted),ENTRY(extremes),{"empty",ascii,0},{"null-empty",NULL,0},{"null-nonempty",NULL,1}};
static UTF_INPUT utf16[]={ENTRY(unicode),ENTRY(lonehigh),ENTRY(lonelow),ENTRY(badpair),ENTRY(maxunicode),{"odd",unicode,3},{"odd-one",unicode,1},{"empty",unicode,0},{"null-empty",NULL,0},{"null-nonempty",NULL,2}};
static CONVERT parallelFrom,parallelTo;static BOOL lookupControl,externalObservation;
typedef struct {PVOID lock;HANDLE semaphore;BOOL read;} LOADER_STATE;
static LOADER_STATE loaderStates[2][2];static DWORD phaseCounts[2][2];
static LOADER_STATE loader_state(void){
    LOADER_STATE state={0};BYTE *peb;
    /* Explicit NT6 diagnostic mode only. Memory loads, no loader/object API. */
    __try{
#ifdef _WIN64
        peb=(BYTE*)__readgsqword(0x60);
        state.lock=*(PVOID volatile *)(peb+0x110);
#else
        peb=(BYTE*)__readfsdword(0x30);
        state.lock=*(PVOID volatile *)(peb+0xa0);
#endif
        if(state.lock){state.semaphore=((volatile RTL_CRITICAL_SECTION*)state.lock)->LockSemaphore;state.read=TRUE;}
    }__except(EXCEPTION_EXECUTE_HANDLER){state.read=FALSE;}
    return state;
}
static DWORD WINAPI parallel_worker(PVOID ignored) {
    WCHAR decoded[16];BYTE encoded[32];ULONG actual,i;LONG status;DWORD errors=0;
    static const WCHAR expected[]={0x41,0xa2,0x65e5,0xd83d,0xde00,0};
    (void)ignored;
    for(i=0;i<1000;++i) {
        if(lookupControl) {
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlUTF8ToUnicodeN");
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlUnicodeToUTF8N");
            continue;
        }
        seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);
        status=parallelFrom(decoded,sizeof(decoded),&actual,valid,sizeof(valid));
        if(status || actual!=sizeof(expected) || memcmp(decoded,expected,sizeof(expected)) || GetLastError()!=0x13579bdf || getstatus()!=(LONG)0xc0000022)++errors;
        status=parallelTo(encoded,sizeof(encoded),&actual,expected,sizeof(expected));
        if(status || actual!=sizeof(valid) || memcmp(encoded,valid,sizeof(valid)) || GetLastError()!=0x13579bdf || getstatus()!=(LONG)0xc0000022)++errors;
        status=parallelFrom(NULL,0,&actual,valid,sizeof(valid));if(status || actual!=sizeof(expected))++errors;
        status=parallelTo(NULL,0,&actual,expected,sizeof(expected));if(status || actual!=sizeof(valid))++errors;
    }
    return errors;
}
static void parallel_cases(CONVERT from,CONVERT to,unsigned phase) {
    HANDLE threads[4];DWORD before,after,immediate,wait,errors,total=0;unsigned i;
    parallelFrom=from;parallelTo=to;
#ifdef KXNT_UTF8_RESOURCE_TRACE
    if(!phase)utf8_snapshot("before");
#endif
#ifdef KXNT_IFEO_RESOURCE_PHASES
    KxNtIfeoResourcePhase(phase*2);
#endif
    GetProcessHandleCount(GetCurrentProcess(),&before);
    if(externalObservation){phaseCounts[phase][0]=before;loaderStates[phase][0]=loader_state();}
    for(i=0;i<4;++i)threads[i]=CreateThread(NULL,0,parallel_worker,NULL,0,NULL);
    wait=WaitForMultipleObjects(4,threads,TRUE,10000);
    if(wait!=WAIT_OBJECT_0){fprintf(out,"Owned UTF workers failed to finish\n");fflush(out);TerminateProcess(GetCurrentProcess(),9);}
    for(i=0;i<4;++i){GetExitCodeThread(threads[i],&errors);total+=errors;CloseHandle(threads[i]);}
    GetProcessHandleCount(GetCurrentProcess(),&immediate);
#ifdef KXNT_UTF8_RESOURCE_TRACE
    if(!phase)utf8_snapshot("immediate");
#endif
    Sleep(100);GetProcessHandleCount(GetCurrentProcess(),&after);
    if(externalObservation){phaseCounts[phase][1]=after;loaderStates[phase][1]=loader_state();}
#ifdef KXNT_IFEO_RESOURCE_PHASES
    KxNtIfeoResourcePhase(phase*2+1);
#endif
    fprintf(out,"HandleObservation Phase=%u ImmediateDelta=%ld After100msDelta=%ld Control=%d\n",phase,(LONG)immediate-(LONG)before,(LONG)after-(LONG)before,lookupControl);
    fprintf(out,"Parallel=4 Phase=%u Calls=%u Errors=%lu HandleDelta=%ld\n",phase,lookupControl?8000:16000,total,(LONG)after-(LONG)before);
    if(total || (phase && after!=before))++failures;
#ifdef KXNT_UTF8_RESOURCE_TRACE
    if(!phase){utf8_snapshot("after100ms");Sleep(900);utf8_snapshot("after1000ms");}
    else utf8_snapshot("warm100ms");
#endif
}
static void overlap_cases(const char *direction,CONVERT fn,const void *original,ULONG bytes) {
    BYTE buffer[128];int offset;ULONG actual,i;LONG status,exception;
    for(offset=-4;offset<=4;++offset) {
        memset(buffer,0xa5,sizeof(buffer));memcpy(buffer+32,original,bytes);actual=0xdeadbeef;status=0xdeadbeef;exception=0;
        seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);
        __try{status=fn(buffer+32+offset,32,&actual,buffer+32,bytes);}__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
        fprintf(out,"Overlap=%s Offset=%d Status=%08lx Exception=%08lx Actual=%08lx Error=%08lx TLS=%08lx Data=",direction,offset,status,exception,actual,GetLastError(),getstatus());
        for(i=28;i<68;++i)fprintf(out,"%02x",buffer[i]);fputc('\n',out);
        if(GetLastError()!=0x13579bdf || getstatus()!=(LONG)0xc0000022)++failures;
    }
}
static void random_cases(CONVERT from,CONVERT to) {
    ULONG seed=0x69133742,i,j;BYTE bytes[32];WCHAR wide[16];char name[32];UTF_INPUT input;
    for(i=0;i<1024;++i) {
        for(j=0;j<32;++j){seed=seed*1664525+1013904223;bytes[j]=(BYTE)(seed>>24);}
        for(j=0;j<16;++j){seed=seed*1664525+1013904223;wide[j]=(WCHAR)(seed>>16);}
        sprintf(name,"random-%lu",i);input.Name=name;input.Data=bytes;input.Bytes=i%33;
        call("decode",from,&input,i%33,FALSE,TRUE);call("decode",from,&input,0,TRUE,TRUE);
        input.Data=wide;input.Bytes=(i%17)*2;
        call("encode",to,&input,i%33,FALSE,TRUE);call("encode",to,&input,0,TRUE,TRUE);
    }
}
static void pointer_cases(const char *direction,CONVERT fn) {
    BYTE *page=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);BYTE destination[32];
    DWORD old,error;ULONG i,actual,capacity,bytes;LONG status,exception,tls;PVOID dest;const void *source;PULONG count;
    VirtualProtect(page+4096,4096,PAGE_NOACCESS,&old);
    for(i=0;i<12;++i) {
        memset(destination,0xa5,sizeof(destination));actual=0xdeadbeef;dest=destination;source=unicode;count=&actual;capacity=32;bytes=2;
        switch(i) {
        case 0:source=(PVOID)1;break;
        case 1:source=page+4096;break;
        case 2:source=page+4096;capacity=0;break;
        case 3:source=page+4096;bytes=0;break;
        case 4:dest=(PVOID)1;break;
        case 5:dest=page+4096;break;
        case 6:dest=page+4096;capacity=0;break;
        case 7:count=(PULONG)1;break;
        case 8:count=(PULONG)(page+4096);break;
        case 9:count=(PULONG)(page+4096-2);break;
        case 10:source=page+4096-1;page[4095]=0xc2;break;
        case 11:dest=NULL;source=page+4096;break;
        }
        seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);status=0xdeadbeef;exception=0;
        __try{status=fn(dest,capacity,count,source,bytes);}__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
        error=GetLastError();tls=getstatus();
        fprintf(out,"Pointer=%s Index=%lu Status=%08lx Exception=%08lx Actual=%08lx Error=%08lx TLS=%08lx First=%02x%02x%02x%02x\n",direction,i,status,exception,actual,error,tls,destination[0],destination[1],destination[2],destination[3]);
        if(error!=0x13579bdf || tls!=(LONG)0xc0000022)++failures;
    }
    VirtualFree(page,0,MEM_RELEASE);
}
static void scalar_cases(CONVERT from,CONVERT to) {
    HANDLE heap=GetProcessHeap();ULONG code,units=0,actual=0,query=0;LONG status;int length;BOOL ok;
    WCHAR *wide=HeapAlloc(heap,0,0x110000*4),*back=HeapAlloc(heap,0,0x110000*4);
    BYTE *expected=HeapAlloc(heap,0,0x110000*4),*encoded=HeapAlloc(heap,0,0x110000*4);
    if(!wide || !back || !expected || !encoded){++failures;goto Cleanup;}
    for(code=0;code<=0x10ffff;++code) {
        if(code>=0xd800 && code<=0xdfff)continue;
        if(code<0x10000)wide[units++]=(WCHAR)code;
        else {ULONG value=code-0x10000;wide[units++]=(WCHAR)(0xd800+(value>>10));wide[units++]=(WCHAR)(0xdc00+(value&0x3ff));}
    }
    // Independent Win32 conversion supplies the full expected UTF-8 bytes.
    length=WideCharToMultiByte(CP_UTF8,0,wide,units,(LPSTR)expected,0x110000*4,NULL,NULL);
    if(!length){++failures;goto Cleanup;}
    status=to(encoded,0x110000*4,&actual,wide,units*2);ok=status==0 && actual==(ULONG)length && memcmp(encoded,expected,length)==0;
    status=to(NULL,0,&query,wide,units*2);ok=ok && status==0 && query==(ULONG)length;
    status=from(back,0x110000*4,&actual,expected,length);ok=ok && status==0 && actual==units*2 && memcmp(back,wide,units*2)==0;
    status=from(NULL,0,&query,expected,length);ok=ok && status==0 && query==units*2;
    fprintf(out,"Scalars=1112064 UTF16Bytes=%lu UTF8Bytes=%d CompleteContent=%d\n",units*2,length,ok);
    if(!ok)++failures;
Cleanup:
    if(wide)HeapFree(heap,0,wide);if(back)HeapFree(heap,0,back);if(expected)HeapFree(heap,0,expected);if(encoded)HeapFree(heap,0,encoded);
}
int main(int argc,char **argv) {
    HMODULE module,native=GetModuleHandleW(L"ntdll.dll");CONVERT from,to;unsigned i;ULONG cap;char path[MAX_PATH];
    if(argc!=3 && argc!=4)return 2;out=fopen(argv[2],"w");if(!out)return 3;
    lookupControl=argc==4 && (!strcmp(argv[3],"lookup-control") || !strcmp(argv[3],"lookup-control-observe"));
    externalObservation=argc==4 && (!strcmp(argv[3],"external-observation") || !strcmp(argv[3],"lookup-control-observe"));
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    getstatus=(GET_STATUS)GetProcAddress(native,"RtlGetLastNtStatus");seedstatus=(SET_STATUS)GetProcAddress(native,"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    module=!strcmp(argv[1],"native")?native:LoadLibraryA(argv[1]);if(!module || !getstatus || !seedstatus)return 4;
    GetModuleFileNameA(module,path,sizeof(path));fprintf(out,"Bits=%u Provider=%s\n",(unsigned)(sizeof(void*)*8),path);
    from=(CONVERT)GetProcAddress(module,"RtlUTF8ToUnicodeN");to=(CONVERT)GetProcAddress(module,"RtlUnicodeToUTF8N");
    if(!from || !to){fprintf(out,"ExportsPresent=0\n");fclose(out);return 5;}
    if(GetModuleHandleW(L"KexDll.dll")){GetModuleFileNameA(GetModuleHandleW(L"KexDll.dll"),path,sizeof(path));fprintf(out,"Implementation=%s\n",path);}
    fprintf(out,"ExportsPresent=1\n");
    parallel_cases(from,to,0);parallel_cases(from,to,1);
    /* External observer reads only after both original resource measurements.
       No module/object query is added inside the measured phases. */
    if(externalObservation){
        for(i=0;i<2;++i)fprintf(out,"InlineLoaderState Phase=%u BeforeLock=%p BeforeSemaphore=%p BeforeRead=%d AfterLock=%p AfterSemaphore=%p AfterRead=%d BeforeHandles=%lu AfterHandles=%lu\n",i,loaderStates[i][0].lock,loaderStates[i][0].semaphore,loaderStates[i][0].read,loaderStates[i][1].lock,loaderStates[i][1].semaphore,loaderStates[i][1].read,phaseCounts[i][0],phaseCounts[i][1]);
        fprintf(out,"ExternalObservationReady=1 PID=%lu\n",GetCurrentProcessId());fflush(out);Sleep(3000);
    }
    if(lookupControl){fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"CONTROL");fclose(out);return failures?1:0;}
    for(i=0;i<sizeof(utf8)/sizeof(utf8[0]);++i){for(cap=0;cap<=24;++cap)call("decode",from,&utf8[i],cap,FALSE,TRUE);call("decode",from,&utf8[i],0,TRUE,TRUE);call("decode",from,&utf8[i],1,TRUE,TRUE);call("decode",from,&utf8[i],32,FALSE,FALSE);call("decode",from,&utf8[i],0,TRUE,FALSE);}
    for(i=0;i<sizeof(utf16)/sizeof(utf16[0]);++i){for(cap=0;cap<=24;++cap)call("encode",to,&utf16[i],cap,FALSE,TRUE);call("encode",to,&utf16[i],0,TRUE,TRUE);call("encode",to,&utf16[i],1,TRUE,TRUE);call("encode",to,&utf16[i],32,FALSE,FALSE);call("encode",to,&utf16[i],0,TRUE,FALSE);}
    random_cases(from,to);pointer_cases("decode",from);pointer_cases("encode",to);scalar_cases(from,to);
    overlap_cases("decode",from,valid,sizeof(valid));overlap_cases("encode",to,unicode,sizeof(unicode));
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
