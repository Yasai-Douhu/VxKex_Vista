#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef LONG (WINAPI *COMPARE_FN)(HANDLE,HANDLE);
typedef LONG (WINAPI *QUERY_FN)(ULONG,PVOID,ULONG,PULONG);
typedef struct { PVOID object; ULONG_PTR pid,handle; ULONG access; USHORT trace,type; ULONG flags,reserved; } ENTRY;
typedef struct { ULONG_PTR count,reserved; ENTRY entries[1]; } TABLE;
static FILE *out;
static COMPARE_FN compare, alias;
static unsigned cases, failures;
static void diagnosticHandles(const char *phase)
{
    typedef LONG (WINAPI *OBJECT_FN)(HANDLE,ULONG,PVOID,ULONG,PULONG);
    typedef LONG (WINAPI *PROCESS_FN)(HANDLE,ULONG,PVOID,ULONG,PULONG);
    struct { USHORT length,maximum; WCHAR *buffer; } *name;
    QUERY_FN query=(QUERY_FN)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQuerySystemInformation");
    OBJECT_FN object=(OBJECT_FN)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQueryObject");
    PROCESS_FN process=(PROCESS_FN)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQueryInformationProcess");
    ULONG bytes=4*1024*1024,needed;
    TABLE *table=(TABLE*)malloc(bytes);
    ULONG_PTR i;
    BYTE buffer[4096];
    BYTE nameBuffer[4096];
    if(table && query(64,table,bytes,&needed)>=0){
        for(i=0;i<table->count && i<(bytes-2*sizeof(ULONG_PTR))/sizeof(ENTRY);++i){
            ENTRY *entry=&table->entries[i];
            if(entry->pid!=GetCurrentProcessId())continue;
            if(object((HANDLE)entry->handle,2,buffer,sizeof(buffer),&needed)<0)continue;
            name=(void*)buffer;
            fprintf(out,"Diagnostic %s Handle=%Ix Object=%p Type=%.*ls\n",phase,entry->handle,entry->object,name->length/2,name->buffer);
            if(name->length==6 && memcmp(name->buffer,L"Key",6)==0 &&
               object((HANDLE)entry->handle,1,nameBuffer,sizeof(nameBuffer),&needed)>=0){
                name=(void*)nameBuffer;
                fprintf(out,"Diagnostic %s Key=%.*ls\n",phase,name->length/2,name->buffer);
            } else if(name->length==14 && memcmp(name->buffer,L"Process",14)==0 &&
               process((HANDLE)entry->handle,27,nameBuffer,sizeof(nameBuffer),&needed)>=0){
                name=(void*)nameBuffer;
                fprintf(out,"Diagnostic %s ProcessImage=%.*ls\n",phase,name->length/2,name->buffer);
            }
        }
    }
    free(table);
}
typedef struct { HANDLE gate,run,done,exit,first,duplicate,other; unsigned failures; int mode; } WORKER;
static DWORD WINAPI worker(void *argument)
{
    WORKER *context=(WORKER*)argument;
    unsigned i,phase;
    if(WaitForSingleObject(context->gate,10000)!=WAIT_OBJECT_0){++context->failures;return 1;}
    for(phase=0;phase<2;++phase){
        if(phase && WaitForSingleObject(context->run,10000)!=WAIT_OBJECT_0){++context->failures;return 1;}
        if(context->mode==5 || context->mode==6)Sleep(2000);
        for(i=0;i<100;++i){
            if(context->mode==1 || context->mode==5 || context->mode==6)continue;
            if(context->mode==2){
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCompareObjects");
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtWow64QueryInformationProcess64");
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtWow64ReadVirtualMemory64");
                continue;
            }
            if(context->mode==3 || context->mode==4){
                void *memory=HeapAlloc(GetProcessHeap(),0,65536);
                if(!memory){++context->failures;continue;}
                if(context->mode==4){
                    QUERY_FN query=(QUERY_FN)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQuerySystemInformation");
                    ULONG returned;
                    LONG status=query(64,memory,65536,&returned);
                    if(status<0 && (ULONG)status!=0xc0000004)++context->failures;
                }
                HeapFree(GetProcessHeap(),0,memory);
                continue;
            }
            if(compare(context->first,context->duplicate)!=0)++context->failures;
            if((ULONG)alias(context->first,context->other)!=0xc00001ac)++context->failures;
        }
        SetEvent(context->done);
    }
    if(WaitForSingleObject(context->exit,10000)!=WAIT_OBJECT_0){++context->failures;return 1;}
    return 0;
}
static void test(const char *name, HANDLE first, HANDLE second, ULONG expected)
{
    ULONG result, secondResult, error;
    SetLastError(0x12345678);
    result=(ULONG)compare(first,second);
    error=GetLastError();
    secondResult=alias ? (ULONG)alias(first,second) : result;
    fprintf(out,"Case=%s Status=%08lx AliasStatus=%08lx LastError=%08lx\n",name,result,secondResult,error);
    if (result != expected || secondResult != expected || error != 0x12345678) ++failures;
    ++cases;
}
int main(int argc,char **argv)
{
    HMODULE module;
    HANDLE first,second,dup,secondDup,secondZero,named,reopened,mutex,process,zeroAccess,thread,file1,file2,fileDup,closed;
    WCHAR name[80],path[MAX_PATH],folder[MAX_PATH];
    QUERY_FN query;
    ULONG size=4*1024*1024,returned=0;
    LONG status;
    TABLE *table;
    ULONG_PTR i;
    int consoleOnly;
    if (argc < 3 || argc > 4) return 2;
    consoleOnly=argc==4 && strcmp(argv[3],"ConsoleOnly")==0;
    out=fopen(argv[2],"w"); if(!out)return 3;
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    if(!consoleOnly){
        module=LoadLibraryA(argv[1]); if(!module)return 4;
        compare=(COMPARE_FN)GetProcAddress(module,"NtCompareObjects");
        alias=(COMPARE_FN)GetProcAddress(module,"ZwCompareObjects");
        if(!compare || (!alias && argc != 4)){fprintf(out,"ExportMissing\n");fclose(out);return 5;}
    }
    fprintf(out,"Diagnostic KexDllLoaded=%u\n",GetModuleHandleW(L"KexDll.dll")!=NULL);
    first=CreateEvent(NULL,TRUE,FALSE,NULL); second=CreateEvent(NULL,TRUE,FALSE,NULL);
    mutex=CreateMutex(NULL,FALSE,NULL);
    swprintf(name,80,L"Local\\KxNtParityCompare-%lu",GetCurrentProcessId());
    named=CreateEventW(NULL,TRUE,FALSE,name); reopened=OpenEventW(SYNCHRONIZE,FALSE,name);
    if(!DuplicateHandle(GetCurrentProcess(),first,GetCurrentProcess(),&dup,0,FALSE,DUPLICATE_SAME_ACCESS) ||
       !DuplicateHandle(GetCurrentProcess(),second,GetCurrentProcess(),&secondDup,0,FALSE,DUPLICATE_SAME_ACCESS) ||
       !DuplicateHandle(GetCurrentProcess(),second,GetCurrentProcess(),&secondZero,0,FALSE,0) ||
       !DuplicateHandle(GetCurrentProcess(),first,GetCurrentProcess(),&zeroAccess,0,FALSE,0) ||
       !DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&process,0,FALSE,0) ||
       !DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&thread,0,FALSE,0)) return 6;
    if(!GetTempPathW(MAX_PATH,folder) || !GetTempFileNameW(folder,L"kxp",0,path))return 7;
    file1=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    file2=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(file1==INVALID_HANDLE_VALUE || file2==INVALID_HANDLE_VALUE ||
       !DuplicateHandle(GetCurrentProcess(),file1,GetCurrentProcess(),&fileDup,0,FALSE,DUPLICATE_SAME_ACCESS))return 8;
    if(alias || consoleOnly){
        HANDLE workers[4], done[4], gate=CreateEvent(NULL,TRUE,FALSE,NULL);
        HANDLE run=CreateEvent(NULL,TRUE,FALSE,NULL),exit=CreateEvent(NULL,TRUE,FALSE,NULL);
        WORKER contexts[4];
        DWORD before,after,initial;
        unsigned threadIndex;
        if(!gate || !run || !exit || !GetProcessHandleCount(GetCurrentProcess(),&initial))return 10;
        diagnosticHandles("initial");
        for(threadIndex=0;threadIndex<4;++threadIndex){
            contexts[threadIndex].gate=gate;contexts[threadIndex].first=first;
            done[threadIndex]=CreateEvent(NULL,TRUE,FALSE,NULL);
            if(!done[threadIndex])return 14;
            contexts[threadIndex].run=run;contexts[threadIndex].done=done[threadIndex];contexts[threadIndex].exit=exit;
            contexts[threadIndex].duplicate=dup;contexts[threadIndex].other=second;
            contexts[threadIndex].failures=0;
            contexts[threadIndex].mode=argc!=4?0:strcmp(argv[3],"NoQuery")==0?1:
                strcmp(argv[3],"LookupOnly")==0?2:strcmp(argv[3],"AllocateOnly")==0?3:
                strcmp(argv[3],"SystemOnly")==0?4:strcmp(argv[3],"IdleOnly")==0?5:
                strcmp(argv[3],"ConsoleOnly")==0?6:0;
            workers[threadIndex]=CreateThread(NULL,0,worker,&contexts[threadIndex],0,NULL);
            if(!workers[threadIndex])return 11;
        }
        if(!SetEvent(gate) || WaitForMultipleObjects(4,done,TRUE,60000)!=WAIT_OBJECT_0)return 12;
        if(!GetProcessHandleCount(GetCurrentProcess(),&before))return 13;
        diagnosticHandles("before");
        fprintf(out,"Diagnostic ColdInitializationHandleDelta=%ld\n",(LONG)(before-initial-8));
        for(threadIndex=0;threadIndex<4;++threadIndex)ResetEvent(done[threadIndex]);
        if(!SetEvent(run) || WaitForMultipleObjects(4,done,TRUE,60000)!=WAIT_OBJECT_0)return 12;
        if(!GetProcessHandleCount(GetCurrentProcess(),&after))return 13;
        diagnosticHandles("after");
        if(before!=after)++failures;
        fprintf(out,"ConcurrentCalls=%u HandleDelta=%ld\n",contexts[0].mode?0:800,(LONG)(after-before));
        if(!SetEvent(exit) || WaitForMultipleObjects(4,workers,TRUE,60000)!=WAIT_OBJECT_0)return 12;
        for(threadIndex=0;threadIndex<4;++threadIndex){failures+=contexts[threadIndex].failures;CloseHandle(workers[threadIndex]);CloseHandle(done[threadIndex]);}
        CloseHandle(gate);CloseHandle(run);CloseHandle(exit);
    }
    if(!consoleOnly){
    test("same-handle",first,first,0);
    test("duplicate",first,dup,0);
    test("distinct-unnamed-events",first,second,0xc00001ac);
    test("different-types",first,mutex,0xc00001ac);
    test("named-reopened",named,reopened,0);
    test("zero-access-duplicate",first,zeroAccess,0);
    test("zero-access-distinct",zeroAccess,secondZero,0xc00001ac);
    test("pseudo-process",GetCurrentProcess(),process,0);
    test("pseudo-thread",GetCurrentThread(),thread,0);
    test("file-duplicate",file1,fileDup,0);
    test("same-file-separate-opens",file1,file2,0xc00001ac);
    test("null-first",NULL,first,0xc0000008);
    test("null-second",first,NULL,0xc0000008);
    test("null-both",NULL,NULL,0xc0000008);
    test("invalid-same",(HANDLE)(ULONG_PTR)0x123456,(HANDLE)(ULONG_PTR)0x123456,0xc0000008);
    closed=CreateEvent(NULL,TRUE,FALSE,NULL); CloseHandle(closed);
    test("closed-handle",closed,first,0xc0000008);
    }
    // Diagnostic only: compare the WOW64/native layouts without treating a
    // truncated kernel pointer as proof of complete object identity.
    query=(QUERY_FN)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQuerySystemInformation");
    table=(TABLE*)malloc(size);
    if(!table)return 9;
    status=query(64,table,size,&returned);
    fprintf(out,"Diagnostic TableStatus=%08lx EntryBytes=%u\n",(ULONG)status,(unsigned)sizeof(ENTRY));
    if(status>=0)for(i=0;i<table->count && i<(size-2*sizeof(ULONG_PTR))/sizeof(ENTRY);++i){
        if(table->entries[i].pid==GetCurrentProcessId() && table->entries[i].handle==(ULONG_PTR)first){
            fprintf(out,"Diagnostic Object=%p Type=%u\n",table->entries[i].object,table->entries[i].type);break;
        }
    }
    free(table);
    CloseHandle(first);CloseHandle(second);CloseHandle(dup);CloseHandle(secondDup);CloseHandle(secondZero);CloseHandle(named);CloseHandle(reopened);
    CloseHandle(mutex);CloseHandle(process);CloseHandle(thread);CloseHandle(zeroAccess);
    CloseHandle(file1);CloseHandle(file2);CloseHandle(fileDup);DeleteFileW(path);
    fprintf(out,"CompareCases=%u Failures=%u\nResult=%s\n",cases,failures,failures?"FAIL":"PASS");
    fclose(out);return failures ? 1 : 0;
}
