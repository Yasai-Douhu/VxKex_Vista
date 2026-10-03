#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
typedef LONG (WINAPI *QUERY)(ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *OBJECT)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *CONVERT)(PVOID,ULONG,PULONG,const void*,ULONG);
typedef LONG (WINAPI *THREAD_INFO)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef struct {PVOID object;ULONG_PTR pid,handle;ULONG access;USHORT trace,type;ULONG flags,reserved;} ENTRY;
typedef struct {ULONG_PTR count,reserved;ENTRY entries[1];} TABLE;
typedef struct {USHORT length,maximum;PWSTR buffer;} NAME;
static FILE *out;static QUERY query;static OBJECT object;static CONVERT from,to;static int mode;static unsigned failures;
static void snapshot(const char *phase) {
    ULONG capacity=4*1024*1024,needed=0;TABLE *table=NULL;LONG s;ULONG_PTR i,count=0;DWORD measured;
    do {
        table=(TABLE*)malloc(capacity);if(!table){++failures;return;}
        s=query(64,table,capacity,&needed);
        if(s==(LONG)0xc0000004){free(table);table=NULL;capacity*=2;}
    } while(s==(LONG)0xc0000004 && capacity<=32*1024*1024);
    if(s<0){fprintf(out,"Snapshot=%s Status=%08lx\n",phase,s);++failures;free(table);return;}
    if(table->count>(capacity-2*sizeof(ULONG_PTR))/sizeof(ENTRY)){++failures;free(table);return;}
    for(i=0;i<table->count;++i){
        BYTE bytes[4096];NAME *name=(NAME*)bytes;ENTRY *entry=&table->entries[i];
        if(entry->pid!=GetCurrentProcessId())continue;++count;
        s=object((HANDLE)entry->handle,2,bytes,sizeof(bytes),&needed);
        fprintf(out,"Snapshot=%s Handle=%Ix Object=%p Access=%08lx TypeStatus=%08lx",phase,entry->handle,entry->object,entry->access,s);
        if(s>=0)fprintf(out," Type=%.*ls",name->length/2,name->buffer);
        fputc('\n',out);
        if(s>=0 && (name->length==6 && !memcmp(name->buffer,L"Key",6) || name->length==10 && !memcmp(name->buffer,L"Event",10) || name->length==18 && !memcmp(name->buffer,L"Directory",18))){
            BYTE named[4096];NAME *n=(NAME*)named;
            if(object((HANDLE)entry->handle,1,named,sizeof(named),&needed)>=0)fprintf(out,"ObjectName=%s Handle=%Ix Name=%.*ls\n",phase,entry->handle,n->length/2,n->buffer);
        }
        if(s>=0 && name->length==12 && !memcmp(name->buffer,L"Thread",12)){
            THREAD_INFO threadInfo=(THREAD_INFO)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread");PVOID start=NULL;HMODULE module=NULL;char path[MAX_PATH];
            if(threadInfo && threadInfo((HANDLE)entry->handle,9,&start,sizeof(start),NULL)>=0){
                fprintf(out,"ThreadStart=%s Handle=%Ix Address=%p",phase,entry->handle,start);
                if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)start,&module) && GetModuleFileNameA(module,path,sizeof(path)))fprintf(out," Module=%s Offset=%Ix",path,(ULONG_PTR)start-(ULONG_PTR)module);
                fputc('\n',out);
            }
            fprintf(out,"ThreadIdentity=%s Handle=%Ix PID=%lu TID=%lu\n",phase,entry->handle,GetProcessIdOfThread((HANDLE)entry->handle),GetThreadId((HANDLE)entry->handle));
        }
        if(s>=0 && name->length==14 && !memcmp(name->buffer,L"Process",14)){
            char image[MAX_PATH];DWORD size=sizeof(image);
            if(QueryFullProcessImageNameA((HANDLE)entry->handle,0,image,&size))fprintf(out,"ProcessImage=%s Handle=%Ix PID=%lu Image=%s\n",phase,entry->handle,GetProcessId((HANDLE)entry->handle),image);
        }
    }
    free(table);GetProcessHandleCount(GetCurrentProcess(),&measured);
    fprintf(out,"Snapshot=%s Entries=%Iu HandleCount=%lu\n",phase,count,measured);fflush(out);
}
static DWORD WINAPI worker(PVOID ignored) {
    unsigned i;DWORD errors=0;WCHAR decoded[8];BYTE encoded[16];ULONG actual;
    static const BYTE input[]={0x41,0xc2,0xa2,0};static const WCHAR expected[]={0x41,0xa2,0};
    (void)ignored;
    for(i=0;i<1000;++i){
        if(mode==0 || mode==3)continue;
        if(mode==1 || mode==4){GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlUTF8ToUnicodeN");GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlUnicodeToUTF8N");continue;}
        if(from(decoded,sizeof(decoded),&actual,input,sizeof(input)) || actual!=sizeof(expected) || memcmp(decoded,expected,sizeof(expected)))++errors;
        if(to(encoded,sizeof(encoded),&actual,expected,sizeof(expected)) || actual!=sizeof(input) || memcmp(encoded,input,sizeof(input)))++errors;
        if(from(NULL,0,&actual,input,sizeof(input)) || actual!=sizeof(expected))++errors;
        if(to(NULL,0,&actual,expected,sizeof(expected)) || actual!=sizeof(input))++errors;
    }
    return errors;
}
static void run(void) {
    HANDLE threads[4];unsigned i;DWORD errors,wait;
    for(i=0;i<4;++i){threads[i]=CreateThread(NULL,0,worker,NULL,0,NULL);if(!threads[i]){fflush(out);TerminateProcess(GetCurrentProcess(),8);}}
    wait=WaitForMultipleObjects(4,threads,TRUE,10000);
    if(wait!=WAIT_OBJECT_0){fflush(out);TerminateProcess(GetCurrentProcess(),9);}
    for(i=0;i<4;++i){if(!GetExitCodeThread(threads[i],&errors) || errors)++failures;if(!CloseHandle(threads[i]))++failures;}
}
int main(int argc,char **argv) {
    HMODULE native=GetModuleHandleW(L"ntdll.dll"),provider;char path[MAX_PATH];
    if(argc!=4)return 2;mode=atoi(argv[3]);if(mode<0 || mode>4)return 2;
    out=fopen(argv[2],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    query=(QUERY)GetProcAddress(native,"NtQuerySystemInformation");object=(OBJECT)GetProcAddress(native,"NtQueryObject");
    provider=mode>=3?native:LoadLibraryA(argv[1]);if(!query || !object || !provider)return 4;
    from=(CONVERT)GetProcAddress(provider,"RtlUTF8ToUnicodeN");to=(CONVERT)GetProcAddress(provider,"RtlUnicodeToUTF8N");if(mode<3 && (!from || !to))return 5;
    GetModuleFileNameA(provider,path,sizeof(path));fprintf(out,"Bits=%u Mode=%d Provider=%s\n",(unsigned)(sizeof(void*)*8),mode,path);
    fprintf(out,"KexDllLoaded=%d CurrentPID=%lu\n",GetModuleHandleW(L"KexDll.dll")!=NULL,GetCurrentProcessId());
    if(GetModuleHandleW(L"KexDll.dll") && GetModuleFileNameA(GetModuleHandleW(L"KexDll.dll"),path,sizeof(path)))fprintf(out,"Implementation=%s\n",path);
    snapshot("before");run();snapshot("immediate");Sleep(100);snapshot("after100ms");Sleep(900);snapshot("after1000ms");
    run();Sleep(100);snapshot("warm100ms");
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"MEASURED");fclose(out);return failures?1:0;
}
