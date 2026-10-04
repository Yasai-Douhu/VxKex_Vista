// Optional full-case resource instrumentation; no change to default test build.
#include <stdlib.h>
typedef LONG (WINAPI *QUERY)(ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *OBJECT)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *THREAD_INFO)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef struct {PVOID object;ULONG_PTR pid,handle;ULONG access;USHORT trace,type;ULONG flags,reserved;} ENTRY;
typedef struct {ULONG_PTR count,reserved;ENTRY entries[1];} TABLE;
typedef struct {USHORT length,maximum;PWSTR buffer;} NAME;
static void utf8_snapshot(const char *phase) {
    HMODULE native=GetModuleHandleW(L"ntdll.dll");
    QUERY query=(QUERY)GetProcAddress(native,"NtQuerySystemInformation");
    OBJECT object=(OBJECT)GetProcAddress(native,"NtQueryObject");
    ULONG capacity=4*1024*1024,needed=0;TABLE *table=NULL;LONG s;ULONG_PTR i,count=0;DWORD measured;
    /* NT6 Server-clone diagnostic only, never a production layout dependency.
       Read the real PEB LoaderLock pointer, then the published critical-section
       fields. Verify ownership before calling this a native cached semaphore. */
    {
        typedef LONG (WINAPI *BASIC)(HANDLE,ULONG,PVOID,ULONG,PULONG);
        BASIC basic=(BASIC)GetProcAddress(native,"NtQueryInformationProcess");
        PVOID info[6]={0},lock=NULL;SIZE_T read=0;RTL_CRITICAL_SECTION cs;
        HMODULE owner=NULL;char path[MAX_PATH]={0};
#ifdef _WIN64
        const ULONG loaderOffset=0x110;
#else
        const ULONG loaderOffset=0xa0;
#endif
        if(basic && basic(GetCurrentProcess(),0,info,sizeof(info),NULL)>=0 &&
           ReadProcessMemory(GetCurrentProcess(),(BYTE*)info[1]+loaderOffset,&lock,sizeof(lock),&read) && read==sizeof(lock) && lock &&
           ReadProcessMemory(GetCurrentProcess(),lock,&cs,sizeof(cs),&read) && read==sizeof(cs) &&
           GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(PCSTR)lock,&owner) && owner==native &&
           GetModuleFileNameA(owner,path,sizeof(path))) {
            fprintf(out,"LoaderLockSnapshot=%s PEB=%p Lock=%p Semaphore=%p LockCount=%ld Recursion=%ld Owner=%s Offset=%Ix PrivateLayout=NT6-diagnostic\n",phase,info[1],lock,cs.LockSemaphore,cs.LockCount,cs.RecursionCount,path,(ULONG_PTR)lock-(ULONG_PTR)owner);
        } else fprintf(out,"LoaderLockSnapshot=%s Verified=0 Error=%lu PrivateLayout=NT6-diagnostic\n",phase,GetLastError());
    }
    if(!query || !object){++failures;return;}
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
