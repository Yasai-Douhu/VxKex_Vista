#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
/* Same-bitness NT6 Server test observer. No explicit code injection or target
   handle closure. Module snapshots may affect target behavior/timing.
   Private PEB layout is diagnostic only, unrelated to production. */
typedef LONG (WINAPI *QUERY)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *SYSTEM_QUERY)(ULONG,PVOID,ULONG,PULONG);
typedef struct {USHORT length,maximum;PWSTR buffer;} NAME;
typedef struct {PVOID object;ULONG_PTR pid,handle;ULONG access;USHORT trace,type;ULONG flags,reserved;} ENTRY;
typedef struct {ULONG_PTR count,reserved;ENTRY entries[1];} TABLE;
static FILE *out;
static BOOL remote_read(HANDLE process,LPCVOID address,PVOID buffer,SIZE_T bytes){SIZE_T read=0;return ReadProcessMemory(process,address,buffer,bytes,&read) && read==bytes;}
static void critical_sections(HANDLE process,DWORD pid,PRTL_CRITICAL_SECTION_DEBUG first,TABLE *table,HMODULE nativeBase,DWORD nativeSize,PVOID loaderLock){
    PVOID start,cursor;unsigned visited=0,matches=0;BOOL complete=FALSE;
    if(!first || first==(PRTL_CRITICAL_SECTION_DEBUG)(ULONG_PTR)-1){fprintf(out,"ExternalCriticalSections Visited=0 Matches=0 Complete=0\n");return;}
    start=(BYTE*)first+offsetof(RTL_CRITICAL_SECTION_DEBUG,ProcessLocksList);cursor=start;
    do{
        LIST_ENTRY links;RTL_CRITICAL_SECTION_DEBUG debug;RTL_CRITICAL_SECTION lock;ULONG_PTR i;
        PVOID debugAddress=(BYTE*)cursor - offsetof(RTL_CRITICAL_SECTION_DEBUG,ProcessLocksList);
        if(!remote_read(process,cursor,&links,sizeof(links)))break;
        ++visited;
        if(remote_read(process,debugAddress,&debug,sizeof(debug)) && debug.CriticalSection &&
           remote_read(process,debug.CriticalSection,&lock,sizeof(lock)) && lock.DebugInfo==debugAddress && lock.LockSemaphore){
            for(i=0;i<table->count;++i){
                ENTRY *entry=&table->entries[i];
                if(entry->pid==pid && entry->handle==(ULONG_PTR)lock.LockSemaphore){
                    BOOL nativeOwner=(ULONG_PTR)debug.CriticalSection>=(ULONG_PTR)nativeBase && (ULONG_PTR)debug.CriticalSection-(ULONG_PTR)nativeBase<nativeSize;
                    fprintf(out,"ExternalCriticalSection Handle=%Ix Lock=%p Debug=%p NativeNtdllOwner=%d LoaderLock=%d LockCount=%ld Recursion=%ld Contention=%lu\n",entry->handle,debug.CriticalSection,debugAddress,nativeOwner,debug.CriticalSection==loaderLock,lock.LockCount,lock.RecursionCount,debug.ContentionCount);
                    ++matches;break;
                }
            }
        }
        cursor=links.Flink;
        if(cursor==start){complete=TRUE;break;}
    }while(cursor && visited<1024);
    fprintf(out,"ExternalCriticalSections Visited=%u Matches=%u Complete=%d\n",visited,matches,complete);
}
static BOOL ready(PCWSTR path,DWORD pid,unsigned stage){
    FILE *file=_wfopen(path,L"r");char line[512];BOOL found=FALSE;DWORD value,phase;
    if(!file)return FALSE;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"ExternalObservationReady=%lu PID=%lu",&phase,&value)==2 && phase==stage && value==pid){found=TRUE;break;}
    fclose(file);return found;
}
static BOOL observe(HANDLE process,DWORD pid,unsigned stage){
    HMODULE native=GetModuleHandleW(L"ntdll.dll");
    QUERY basic=(QUERY)GetProcAddress(native,"NtQueryInformationProcess"),object=(QUERY)GetProcAddress(native,"NtQueryObject");
    SYSTEM_QUERY system=(SYSTEM_QUERY)GetProcAddress(native,"NtQuerySystemInformation");
    PVOID info[6]={0},lock=NULL;SIZE_T read=0;RTL_CRITICAL_SECTION cs;
    MODULEENTRY32W module;HANDLE modules,duplicate=NULL;BOOL owner=FALSE,typed=FALSE,identity=FALSE;DWORD handles=0;
    BYTE typeBuffer[4096];NAME *type=(NAME*)typeBuffer;ULONG needed=0,capacity=4*1024*1024;TABLE *table=NULL;LONG status;ULONG_PTR i,entries=0;
#ifdef _WIN64
    const ULONG loaderOffset=0x110;
#else
    const ULONG loaderOffset=0xa0;
#endif
    fprintf(out,"ExternalObservationStage=%u\n",stage);
    if(!basic || !object || !system || basic(process,0,info,sizeof(info),NULL)<0 ||
       !ReadProcessMemory(process,(BYTE*)info[1]+loaderOffset,&lock,sizeof(lock),&read) || read!=sizeof(lock) || !lock ||
       !ReadProcessMemory(process,lock,&cs,sizeof(cs),&read) || read!=sizeof(cs))return FALSE;
    modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);ZeroMemory(&module,sizeof(module));module.dwSize=sizeof(module);
    if(modules!=INVALID_HANDLE_VALUE){
        if(Module32FirstW(modules,&module))do{
            if(!_wcsicmp(module.szModule,L"ntdll.dll") && (ULONG_PTR)lock>=(ULONG_PTR)module.modBaseAddr && (ULONG_PTR)lock-(ULONG_PTR)module.modBaseAddr<module.modBaseSize){owner=TRUE;break;}
        }while(Module32NextW(modules,&module));
        CloseHandle(modules);
    }
    if(GetProcessHandleCount(process,&handles))fprintf(out,"ExternalTargetHandleCount=%lu\n",handles);
    fprintf(out,"ExternalLoaderLock PEB=%p Lock=%p Semaphore=%p LockCount=%ld Recursion=%ld NativeOwner=%d PrivateLayout=NT6-diagnostic\n",info[1],lock,cs.LockSemaphore,cs.LockCount,cs.RecursionCount,owner);
    if(owner)fprintf(out,"ExternalNativeModule Base=%p Size=%lu Path=%ls Offset=%Ix\n",module.modBaseAddr,module.modBaseSize,module.szExePath,(ULONG_PTR)lock-(ULONG_PTR)module.modBaseAddr);
    if(!owner)return FALSE;
    if(!cs.LockSemaphore){fprintf(out,"ExternalSemaphore Absent=1\n");typed=TRUE;identity=TRUE;}
    if(cs.LockSemaphore && DuplicateHandle(process,cs.LockSemaphore,GetCurrentProcess(),&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS)){
        status=object(duplicate,2,typeBuffer,sizeof(typeBuffer),&needed);
        /* Query returns a counted name within this caller-owned buffer. */
        if(status>=0 && (BYTE*)type->buffer>=typeBuffer && (BYTE*)type->buffer<=typeBuffer+sizeof(typeBuffer) && type->length<=sizeof(typeBuffer)-(SIZE_T)((BYTE*)type->buffer-typeBuffer)){
            typed=type->length==10 && !memcmp(type->buffer,L"Event",10);
            fprintf(out,"ExternalSemaphore Type=%.*ls Status=%08lx DuplicateClosed=1\n",type->length/2,type->buffer,status);
        }
        CloseHandle(duplicate); /* Only the duplicate owned by the observer. */
    }
    do{
        table=(TABLE*)malloc(capacity);if(!table)return FALSE;
        status=system(64,table,capacity,&needed);
        if(status==(LONG)0xc0000004){free(table);table=NULL;capacity*=2;}
    }while(status==(LONG)0xc0000004 && capacity<=32*1024*1024);
    if(status>=0 && table && table->count<=(capacity-2*sizeof(ULONG_PTR))/sizeof(ENTRY))for(i=0;i<table->count;++i){
        ENTRY *entry=&table->entries[i];if(entry->pid!=pid)continue;
        ++entries;
        fprintf(out,"ExternalHandle PID=%lu Handle=%Ix Object=%p Access=%08lx LoaderSemaphore=%d\n",pid,entry->handle,entry->object,entry->access,entry->handle==(ULONG_PTR)cs.LockSemaphore);
        if(entry->handle==(ULONG_PTR)cs.LockSemaphore)identity=TRUE;
        duplicate=NULL;
        if(DuplicateHandle(process,(HANDLE)entry->handle,GetCurrentProcess(),&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS)){
            status=object(duplicate,2,typeBuffer,sizeof(typeBuffer),&needed);
            if(status>=0 && (BYTE*)type->buffer>=typeBuffer && (BYTE*)type->buffer<=typeBuffer+sizeof(typeBuffer) && type->length<=sizeof(typeBuffer)-(SIZE_T)((BYTE*)type->buffer-typeBuffer)){
                fprintf(out,"ExternalType Handle=%Ix Type=%.*ls\n",entry->handle,type->length/2,type->buffer);
                if(type->length==12 && !memcmp(type->buffer,L"Thread",12)){
                    QUERY thread=(QUERY)GetProcAddress(native,"NtQueryInformationThread");PVOID startAddress=NULL;DWORD threadExit=0xffffffff;
                    GetExitCodeThread(duplicate,&threadExit);
                    if(thread && thread(duplicate,9,&startAddress,sizeof(startAddress),NULL)>=0)fprintf(out,"ExternalThread Handle=%Ix ID=%lu Start=%p Exit=%08lx\n",entry->handle,GetThreadId(duplicate),startAddress,threadExit);
                }
                /* Never query arbitrary File names: drivers may block queries. */
                if(type->length==10 && !memcmp(type->buffer,L"Event",10)){
                    BYTE nameBuffer[4096];NAME *name=(NAME*)nameBuffer;
                    if(object(duplicate,1,nameBuffer,sizeof(nameBuffer),&needed)>=0 && (BYTE*)name->buffer>=nameBuffer && (BYTE*)name->buffer<=nameBuffer+sizeof(nameBuffer) && name->length<=sizeof(nameBuffer)-(SIZE_T)((BYTE*)name->buffer-nameBuffer))fprintf(out,"ExternalEventName Handle=%Ix Name=%.*ls\n",entry->handle,name->length/2,name->buffer);
                }
            }
            CloseHandle(duplicate);
        }
    }
    fprintf(out,"ExternalTable Entries=%Iu\n",entries);
    if(status>=0 && table && table->count<=(capacity-2*sizeof(ULONG_PTR))/sizeof(ENTRY))critical_sections(process,pid,cs.DebugInfo,table,(HMODULE)module.modBaseAddr,module.modBaseSize,lock);
    fflush(out);
    free(table);return owner && typed && identity;
}
int wmain(int argc,WCHAR **argv){
    STARTUPINFOW si;PROCESS_INFORMATION pi;WCHAR command[4096];OSVERSIONINFOEXW version;
    DWORD start,wait,exitCode=0xffffffff;unsigned stage;BOOL verified=TRUE,sameBits=FALSE,targetWow=FALSE,selfWow=FALSE,seen;
    /* image provider target-log observer-log adapter|control */
    if(argc!=6 || (wcscmp(argv[5],L"adapter") && wcscmp(argv[5],L"control")))return 87;
    out=_wfopen(argv[4],L"w");if(!out)return 2;
    ZeroMemory(&version,sizeof(version));version.dwOSVersionInfoSize=sizeof(version);GetVersionExW((OSVERSIONINFOW*)&version);
    fprintf(out,"ObserverBits=%u KexDllLoaded=%d OS=%lu.%lu ProductType=%u\n",(unsigned)(sizeof(void*)*8),GetModuleHandleW(L"KexDll.dll")!=NULL,version.dwMajorVersion,version.dwMinorVersion,version.wProductType);
    if(version.dwMajorVersion!=6 || version.dwMinorVersion!=0 || version.wProductType!=VER_NT_SERVER || GetModuleHandleW(L"KexDll.dll")){fclose(out);return 87;}
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    _snwprintf(command,4096,L"\"%s\" \"%s\" \"%s\" %s",argv[1],argv[2],argv[3],!wcscmp(argv[5],L"control")?L"lookup-control-observe":L"external-observation");command[4095]=0;
    ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);ZeroMemory(&pi,sizeof(pi));
    if(!CreateProcessW(argv[1],command,NULL,NULL,FALSE,0,NULL,L"C:\\Windows",&si,&pi)){fprintf(out,"CreateProcessError=%lu\n",GetLastError());fclose(out);return 3;}
    fprintf(out,"KXNT_TARGET_PID=%lx\n",pi.dwProcessId);fflush(out);
    sameBits=IsWow64Process(GetCurrentProcess(),&selfWow) && IsWow64Process(pi.hProcess,&targetWow) && selfWow==targetWow;
    for(stage=1;stage<=2;++stage){
        seen=FALSE;start=GetTickCount();
        while(sameBits && GetTickCount()-start<20000 && WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT){
            if(ready(argv[3],pi.dwProcessId,stage)){seen=observe(pi.hProcess,pi.dwProcessId,stage);break;}
            Sleep(25);
        }
        if(!seen){verified=FALSE;break;}
    }
    wait=WaitForSingleObject(pi.hProcess,10000);
    if(wait!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,0xdead);WaitForSingleObject(pi.hProcess,5000);}
    GetExitCodeProcess(pi.hProcess,&exitCode);
    fprintf(out,"SameBitness=%d Observed=%d TargetWait=%08lx TargetExit=%08lx\n",sameBits,verified,wait,exitCode);
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);fclose(out);
    /* Preserve target FAIL; observer failure cannot produce a passing probe. */
    return wait!=WAIT_OBJECT_0?9:(!verified?8:(int)exitCode);
}
