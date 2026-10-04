#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include "../00-Common-Headers/ZigNtIoProfile.h"

typedef struct { LONG Status; ULONG_PTR Information; } IOS;
typedef LONG (WINAPI *WRITE_NT)(HANDLE,HANDLE,PVOID,PVOID,IOS*,PVOID,ULONG,PVOID,PULONG);
typedef LONG (WINAPI *QUERY_OBJECT)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *INIT_DATA)(PVOID*);
typedef LONG (WINAPI *GET_STATUS)(void);
typedef void (WINAPI *SEED_STATUS)(LONG);
typedef struct { USHORT Length,MaximumLength; PWSTR Buffer; } USTRING;
typedef struct { PVOID Object; ULONG_PTR Pid,Handle; ULONG Access; USHORT Trace,Type; ULONG Flags,Reserved; } HANDLE_ENTRY;
typedef struct { ULONG_PTR Count,Reserved; HANDLE_ENTRY Entries[1]; } HANDLE_TABLE;
typedef LONG (WINAPI *QUERY_SYSTEM)(ULONG,PVOID,ULONG,PULONG);
__declspec(dllimport) LONG WINAPI NtWriteFile(HANDLE,HANDLE,PVOID,PVOID,IOS*,PVOID,ULONG,PVOID,PULONG);
__declspec(dllimport) LONG WINAPI NtWaitForAlertByThreadId(PVOID,PVOID);
__declspec(dllimport) LONG WINAPI RtlReportSilentProcessExit(HANDLE,LONG);
#ifndef _WIN64
#pragma comment(linker,"/alternatename:__imp__NtWriteFile@36=__imp__NtWriteFile")
#pragma comment(linker,"/alternatename:__imp__NtWaitForAlertByThreadId@8=__imp__NtWaitForAlertByThreadId")
#pragma comment(linker,"/alternatename:__imp__RtlReportSilentProcessExit@8=__imp__RtlReportSilentProcessExit")
#endif
#pragma section(".buildid", read)
__declspec(allocate(".buildid")) const char ProfileSection[]="Owned NT-I/O profile diagnostic";
static FILE *out;
static unsigned failures;
static BOOL debugZig;static unsigned debugInstance;
static WRITE_NT adapted[2],native;
static QUERY_OBJECT query;
static GET_STATUS getstatus;
static SEED_STATUS seedstatus;
static HANDLE stressScreen;
static BOOL stressNative;
static DWORD WINAPI stressWorker(PVOID argument) {
    unsigned i,alias=(unsigned)(ULONG_PTR)argument;DWORD errors=0;IOS ios;LONG s;
    for(i=0;i<1000;++i){
        if(stressNative){
            HMODULE base=NULL;DWORD written;CPINFO info;WCHAR text;BYTE *capture;
            union { ULONG_PTR Alignment; BYTE Bytes[512]; } type;
            GetFileType(stressScreen);GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"VerifyConsoleIoHandle");query(stressScreen,2,type.Bytes,sizeof(type.Bytes),NULL);
            capture=HeapAlloc(GetProcessHeap(),0,1);if(!capture){++errors;continue;}*capture='H';GetCPInfo(GetConsoleOutputCP(),&info);MultiByteToWideChar(GetConsoleOutputCP(),MB_ERR_INVALID_CHARS,(LPCSTR)capture,1,&text,1);
            GetModuleHandleExW(0,L"KxBase.dll",&base);if(!WriteConsoleW(stressScreen,&text,1,&written,NULL)||written!=1)++errors;if(base)FreeLibrary(base);HeapFree(GetProcessHeap(),0,capture);
        }else{
            seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);s=adapted[alias](stressScreen,NULL,NULL,NULL,&ios,"H",1,NULL,NULL);if(s || ios.Status || ios.Information!=1 || GetLastError()!=0x13579bdf || getstatus()!=(LONG)0xc0000022)++errors;
        }
    }
    return errors;
}
static DWORD partialLimit,partialCalls;
static WCHAR partialInput[8];static DWORD partialInputCount;
static HMODULE partialBase;
static FARPROC (WINAPI *partialResolver)(HMODULE,LPCSTR);
static BOOL WINAPI partialWrite(HANDLE h,LPCVOID text,DWORD count,LPDWORD written,LPVOID reserved) {
    ++partialCalls;
    partialInputCount=count;ZeroMemory(partialInput,sizeof(partialInput));memcpy(partialInput,text,(count<8?count:8)*sizeof(WCHAR));
    return WriteConsoleW(h,text,count>partialLimit?partialLimit:count,written,reserved);
}
static FARPROC WINAPI partialGetProc(HMODULE module,LPCSTR name) {
    if(module==partialBase && (ULONG_PTR)name>0xffff && !strcmp(name,"WriteConsoleW"))return (FARPROC)partialWrite;
    return partialResolver(module,name);
}
static PVOID *namedImport(HMODULE module,const char *name) {
    BYTE *image=(BYTE*)module;IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS*)(image+((IMAGE_DOS_HEADER*)image)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *d=(IMAGE_IMPORT_DESCRIPTOR*)(image+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    // Normal IFEO redirects this probe's WriteConsoleW to KxBase, while the
    // owned KexDll IAT still uses kernel32. Identify its slot by import name.
    for(;d->Name;++d){IMAGE_THUNK_DATA *t=(IMAGE_THUNK_DATA*)(image+d->FirstThunk),*names;
        if(!d->OriginalFirstThunk)continue;names=(IMAGE_THUNK_DATA*)(image+d->OriginalFirstThunk);
        for(;names->u1.AddressOfData;++names,++t){ULONG_PTR rva=(ULONG_PTR)names->u1.AddressOfData;
            if(rva & IMAGE_ORDINAL_FLAG)continue;
            if(rva<=MAXDWORD-2 && ZigIoName(image,nt->OptionalHeader.SizeOfImage,(ULONG)rva+2,name))return (PVOID*)&t->u1.Function;
        }}
    return NULL;
}
static void check(const char *name,BOOL ok) {
    fprintf(out,"Case=%s Pass=%d\n",name,ok);if(!ok)++failures;
}
static unsigned snapshot(HANDLE_ENTRY *entries) {
    QUERY_SYSTEM qs=(QUERY_SYSTEM)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQuerySystemInformation");
    ULONG size=4*1024*1024,needed;HANDLE_TABLE *table=HeapAlloc(GetProcessHeap(),0,size);
    unsigned count=0;ULONG_PTR i;LONG status=table?qs(64,table,size,&needed):(LONG)0xc0000017;
    check("snapshot-query",status>=0);
    if(status>=0){for(i=0;i<table->Count && i<(size-2*sizeof(ULONG_PTR))/sizeof(HANDLE_ENTRY);++i){HANDLE_ENTRY *e=&table->Entries[i];if(e->Pid==GetCurrentProcessId()){if(count==256){check("snapshot-capacity",FALSE);count=0;break;}entries[count++]=*e;}}}
    if(table)HeapFree(GetProcessHeap(),0,table);return count;
}
static BOOL sameentry(const HANDLE_ENTRY *a,const HANDLE_ENTRY *b) {
    return a->Handle==b->Handle && a->Object==b->Object && a->Access==b->Access && a->Type==b->Type && a->Flags==b->Flags;
}
static BOOL samehandles(const HANDLE_ENTRY *a,unsigned na,const HANDLE_ENTRY *b,unsigned nb,BOOL diagnose) {
    unsigned i,j;BOOL same=na==nb;
    for(i=0;i<nb;++i){for(j=0;j<na;++j)if(sameentry(a+j,b+i))break;if(j==na){
        same=FALSE;
        if(diagnose){union{ULONG_PTR Align;BYTE Bytes[512];}buffer;LONG s=query((HANDLE)b[i].Handle,2,buffer.Bytes,sizeof(buffer.Bytes),NULL);USTRING *type=(USTRING*)buffer.Bytes;fprintf(out,"NewHandle=%Ix Type=%.*ls NativeControl=%d\n",b[i].Handle,s>=0?type->Length/2:0,s>=0?type->Buffer:L"",stressNative);}
    }}
    return same;
}
static BOOL filetype(HANDLE h) {
    union { ULONG_PTR Alignment; BYTE Bytes[1024]; } info;
    LONG s=query(h,2,info.Bytes,sizeof(info.Bytes),NULL);
    USTRING *type=(USTRING*)info.Bytes;
    return s>=0 && type->Length==8 && !memcmp(type->Buffer,L"File",8);
}
static BOOL sameios(const IOS *a,const IOS *b) {
    return a->Status==b->Status && a->Information==b->Information;
}
static void profile_bounds(void) {
    BYTE image[4096]; unsigned bits;
    for(bits=4;bits<=8;bits+=4) {
        IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)image;
        IMAGE_FILE_HEADER *file=(IMAGE_FILE_HEADER*)(image+132);
        BYTE *opt=image+152; ULONG dirs=bits==4?96:112;
        IMAGE_SECTION_HEADER *section=(IMAGE_SECTION_HEADER*)(opt+dirs+16);
        IMAGE_IMPORT_DESCRIPTOR *imports=(IMAGE_IMPORT_DESCRIPTOR*)(image+1024);
        ZeroMemory(image,sizeof(image));dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
        *(ULONG*)(image+128)=IMAGE_NT_SIGNATURE;
        file->Machine=bits==4?IMAGE_FILE_MACHINE_I386:IMAGE_FILE_MACHINE_AMD64;
        file->SizeOfOptionalHeader=(USHORT)(dirs+16);file->NumberOfSections=1;
        *(USHORT*)opt=bits==4?IMAGE_NT_OPTIONAL_HDR32_MAGIC:IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        *(ULONG*)(opt+56)=sizeof(image);*(ULONG*)(opt+dirs-4)=2;
        memcpy(section->Name,".buildid",8);section->Misc.VirtualSize=64;section->VirtualAddress=512;section->Characteristics=IMAGE_SCN_MEM_READ;
        *(ULONG*)(opt+dirs+8)=1024;*(ULONG*)(opt+dirs+12)=40;
        imports->Name=1400;imports->OriginalFirstThunk=1280;imports->FirstThunk=1320;
        strcpy((char*)image+1400,"ntdll.dll");
        if(bits==4){ULONG *thunks=(ULONG*)(image+1280);thunks[0]=1560;thunks[1]=1600;thunks[2]=1640;}
        else{ULONGLONG *thunks=(ULONGLONG*)(image+1280);thunks[0]=1560;thunks[1]=1600;thunks[2]=1640;}
        strcpy((char*)image+1562,"NtWriteFile");strcpy((char*)image+1602,"NtWaitForAlertByThreadId");strcpy((char*)image+1642,"RtlReportSilentProcessExit");
        check("profile-fixture",ZigNtIoImage(image,sizeof(image)));
        image[1562]='X';check("profile-missing-import",!ZigNtIoImage(image,sizeof(image)));image[1562]='N';
        section->Name[0]='X';check("profile-no-buildid",!ZigNtIoImage(image,sizeof(image)));section->Name[0]='.';
        section->VirtualAddress=0xfffffff0;check("profile-section-overflow",!ZigNtIoImage(image,sizeof(image)));section->VirtualAddress=512;
        imports->OriginalFirstThunk=0xfffffff0;check("profile-thunk-overflow",!ZigNtIoImage(image,sizeof(image)));imports->OriginalFirstThunk=1280;
        imports->Name=4090;check("profile-dll-bound",!ZigNtIoImage(image,sizeof(image)));imports->Name=1400;
        strcpy((char*)image+1400,"other.dll");check("profile-other-dll",!ZigNtIoImage(image,sizeof(image)));strcpy((char*)image+1400,"KxNt.dll");
        check("profile-rewritten-import",ZigNtIoImage(image,sizeof(image)));
        strcpy((char*)image+1400,"kXnT");check("profile-rewritten-basename",ZigNtIoImage(image,sizeof(image)));
        strcpy((char*)image+1400,"kxntX");check("profile-basename-suffix-rejected",!ZigNtIoImage(image,sizeof(image)));
        strcpy((char*)image+1400,"kxnt.dllX");check("profile-extension-suffix-rejected",!ZigNtIoImage(image,sizeof(image)));
        strcpy((char*)image+1400,"NTDLL");check("profile-native-basename",ZigNtIoImage(image,sizeof(image)));
        strcpy((char*)image+1400,"KxNt.dll");
        memcpy(image+4091,"kxnt",5);imports->Name=4091;check("profile-basename-tail",ZigNtIoImage(image,sizeof(image)));
        image[4095]='X';check("profile-unterminated-tail",!ZigNtIoImage(image,sizeof(image)));imports->Name=1400;
        *(ULONG*)(opt+dirs+12)=20;check("profile-no-end-descriptor",!ZigNtIoImage(image,sizeof(image)));*(ULONG*)(opt+dirs+12)=40;
        file->NumberOfSections=65535;check("profile-section-table-bound",!ZigNtIoImage(image,sizeof(image)));file->NumberOfSections=1;
        dos->e_lfanew=0x7fffffff;check("profile-header-bound",!ZigNtIoImage(image,sizeof(image)));dos->e_lfanew=128;
        check("profile-short-image",!ZigNtIoImage(image,20));
        fprintf(out,"ProfileFixture Bits=%u\n",bits*8);
    }
}
static void collision(HANDLE file) {
    HANDLE screens[128];unsigned count=0,i,alias,standard;HANDLE target=(HANDLE)((ULONG_PTR)file|3);
    HANDLE oldOut=GetStdHandle(STD_OUTPUT_HANDLE),oldErr=GetStdHandle(STD_ERROR_HANDLE);
    IOS ios;LONG s;COORD origin={0,0};WCHAR before[8],after[8];DWORD read;
    while(count<128){HANDLE h=CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CONSOLE_TEXTMODE_BUFFER,NULL);if(h==INVALID_HANDLE_VALUE)break;screens[count++]=h;if(h==target)break;}
    check("collision-found",count && screens[count-1]==target);
    if(count && screens[count-1]==target){
        ReadConsoleOutputCharacterW(target,before,8,origin,&read);
        for(standard=0;standard<3;++standard){
            if(standard==1)check("collision-set-standard-output",SetStdHandle(STD_OUTPUT_HANDLE,target));
            if(standard==2){check("collision-restore-standard-output",SetStdHandle(STD_OUTPUT_HANDLE,oldOut));check("collision-set-standard-error",SetStdHandle(STD_ERROR_HANDLE,target));}
            for(alias=0;alias<2;++alias){memset(&ios,0xa5,sizeof(ios));s=adapted[alias](target,NULL,NULL,NULL,&ios,"C",1,NULL,NULL);check("collision-explicit-error",s==(LONG)0xc00000bb && ios.Status==(LONG)0xa5a5a5a5);}
        }
        check("collision-restore-standard-error",SetStdHandle(STD_ERROR_HANDLE,oldErr));
        ReadConsoleOutputCharacterW(target,after,8,origin,&read);check("collision-console-unchanged",!memcmp(before,after,sizeof(before)));
        check("collision-file-unchanged",GetFileSize(file,NULL)==2);
    }
    for(i=0;i<count;++i)CloseHandle(screens[i]);
}
static void write_only(void) {
    HANDLE list[128],h=INVALID_HANDLE_VALUE;unsigned i,n=0,alias;IOS ios;LONG s;DWORD mode;
    for(i=0;i<128;++i){HANDLE candidate=CreateConsoleScreenBuffer(GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CONSOLE_TEXTMODE_BUFFER,NULL);if(candidate==INVALID_HANDLE_VALUE)break;if(!filetype(candidate)){h=candidate;break;}list[n++]=candidate;}
    check("write-only-found",h!=INVALID_HANDLE_VALUE);
    if(h!=INVALID_HANDLE_VALUE){
        check("write-only-mode-unavailable",!GetConsoleMode(h,&mode));
        for(alias=0;alias<2;++alias){s=adapted[alias](h,NULL,NULL,NULL,&ios,"W",1,NULL,NULL);check("write-only-completion",s==0 && ios.Status==0 && ios.Information==1);}
        CloseHandle(h);
    }
    for(i=0;i<n;++i)CloseHandle(list[i]);
}
static void native_routes(const char *directory) {
    char path[MAX_PATH],byte;HANDLE file,event,files[128];unsigned n=0,i,alias;
    IOS ios,ref;LONG s,t;LONGLONG offset=0;DWORD read;
    sprintf(path,"%s\\condrv-native.tmp",directory);
    file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    event=CreateEventW(NULL,TRUE,FALSE,NULL);
    check("native-route-resources",file!=INVALID_HANDLE_VALUE && event!=NULL);
    {union{ULONG_PTR Align;BYTE Bytes[64];}storage;for(i=0;i<8;++i){IOS *p=(IOS*)(storage.Bytes+i);memset(&storage,0xa5,sizeof(storage));s=native(file,NULL,NULL,NULL,p,"A",1,&offset,NULL);fprintf(out,"NativeIOSAlignment=%u Status=%08lx\n",i,(unsigned long)s);}}
    for(alias=0;alias<2;++alias){
        memset(&ios,0xa5,sizeof(ios));ref=ios;ResetEvent(event);
        s=adapted[alias](file,event,NULL,NULL,&ios,"E",1,&offset,NULL);
        check("native-event-complete",s==0 && ios.Status==0 && ios.Information==1 && WaitForSingleObject(event,0)==WAIT_OBJECT_0);
        ResetEvent(event);t=native(file,event,NULL,NULL,&ref,"E",1,&offset,NULL);
        check("native-event-match",s==t && sameios(&ios,&ref) && WaitForSingleObject(event,0)==WAIT_OBJECT_0);
    }
    // Find a tagged File handle that is not also a live console. Retain all
    // candidates until the test ends so handle allocation cannot repeat.
    for(i=0;i<128;++i){HANDLE h=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)break;files[n++]=h;if(GetFileType((HANDLE)((ULONG_PTR)h|3))!=FILE_TYPE_CHAR)break;}
    check("native-tagged-file-found",n && i<128);
    if(n && i<128){for(alias=0;alias<2;++alias){s=adapted[alias]((HANDLE)((ULONG_PTR)files[n-1]|3),NULL,NULL,NULL,&ios,"T",1,&offset,NULL);check("native-tagged-file-write",s==0 && ios.Information==1);}}
    SetFilePointer(file,0,NULL,FILE_BEGIN);check("native-tagged-file-content",ReadFile(file,&byte,1,&read,NULL) && read==1 && byte=='T');
    for(i=0;i<n;++i)CloseHandle(files[i]);CloseHandle(event);CloseHandle(file);DeleteFileA(path);
}
static void stress(HANDLE screen) {
    HANDLE threads[4];DWORD before,after,wait,errors;unsigned i,phase;UINT cp=GetConsoleOutputCP();
    HANDLE_ENTRY oldentries[256],newentries[256];unsigned oldcount,newcount;
    SetConsoleOutputCP(437);stressScreen=screen;Sleep(2000);GetProcessHandleCount(GetCurrentProcess(),&before);
    oldcount=snapshot(oldentries);
    for(phase=0;phase<2;++phase){
    for(i=0;i<4;++i)threads[i]=CreateThread(NULL,0,stressWorker,(PVOID)(ULONG_PTR)(i&1),0,NULL);
    wait=WaitForMultipleObjects(4,threads,TRUE,30000);check("concurrent-writes-finished",wait==WAIT_OBJECT_0);
    if(wait!=WAIT_OBJECT_0){fprintf(out,"Incomplete workers: refusing to close live worker resources\n");fflush(out);ExitProcess(9);}
    for(i=0;i<4;++i){GetExitCodeThread(threads[i],&errors);check("concurrent-write-errors",errors==0);CloseHandle(threads[i]);}
    GetProcessHandleCount(GetCurrentProcess(),&after);
    newcount=snapshot(newentries);
    if(phase)check("concurrent-handle-delta",after==before);
    if(phase)check("concurrent-handle-identities",samehandles(oldentries,oldcount,newentries,newcount,TRUE));
    else samehandles(oldentries,oldcount,newentries,newcount,TRUE);
    fprintf(out,"ConcurrentWrites=4000 Threads=4 Phase=%u NativeControl=%d HandleDelta=%ld\n",phase,stressNative,(LONG)after-(LONG)before);
    before=after;
    memcpy(oldentries,newentries,newcount*sizeof(HANDLE_ENTRY));oldcount=newcount;
    }
    SetConsoleOutputCP(cp);
}
static BOOL cells(HANDLE screen,WCHAR *text) {
    CHAR_INFO info[16]; COORD size={16,1},origin={0,0}; SMALL_RECT area={0,0,15,0};unsigned i;
    if(!ReadConsoleOutputW(screen,info,size,origin,&area))return FALSE;
    for(i=0;i<16;++i)text[i]=info[i].Char.UnicodeChar;
    return area.Left==0 && area.Right==15 && area.Top==0 && area.Bottom==0;
}
static void display(HANDLE screen,UINT cp,const BYTE *text,ULONG length,const WCHAR *expected,unsigned n) {
    COORD origin={0,0}; WCHAR result[64],reference[64]; DWORD fill,written; IOS ios; LONG s;
    UINT oldcp=GetConsoleOutputCP(); unsigned alias;
    check("set-codepage",SetConsoleOutputCP(cp));
    FillConsoleOutputCharacterW(screen,L' ',64,origin,&fill);
    SetConsoleCursorPosition(screen,origin);
    check("native-unicode-reference",WriteConsoleW(screen,expected,n,&written,NULL) && written==n);
    check("native-unicode-read",cells(screen,reference));
    for(alias=0;alias<2;++alias) {
        FillConsoleOutputCharacterW(screen,L' ',64,origin,&fill);
        SetConsoleCursorPosition(screen,origin);
        memset(&ios,0xa5,sizeof(ios)); seedstatus((LONG)0xc0000022); SetLastError(0x13579bdf);
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,(PVOID)text,length,NULL,NULL);
        check("console-byte-completion",s==0 && ios.Status==0 && ios.Information==length);
        check("console-tls",GetLastError()==0x13579bdf && getstatus()==(LONG)0xc0000022);
        ZeroMemory(result,sizeof(result));
        check("console-output-read",cells(screen,result) && !memcmp(result,reference,16*2));
        fprintf(out,"Display CP=%u Alias=%u InputBytes=%lu ExpectedUnits=%u Status=%08lx Bytes=%Iu\n",cp,alias,length,n,(unsigned long)s,ios.Information);
        {unsigned j;fprintf(out,"ConsoleCells=");for(j=0;j<16;++j)fprintf(out,"%04x,",result[j]);fprintf(out,"\nNativeCells=");for(j=0;j<16;++j)fprintf(out,"%04x,",reference[j]);fprintf(out,"\n");}
    }
    SetConsoleOutputCP(oldcp);
}
static void partial_cases(HMODULE kex,HANDLE screen) {
    const BYTE bytes[]={0x41,0xe6,0x97,0xa5,0xf0,0x9f,0x98,0x80};
    const WCHAR prefix[]={L'A',0x65e5},whole[]={L'A',0x65e5,0xd83d,0xde00};WCHAR reference[16],actual[16];
    COORD origin={0,0};IOS ios;DWORD oldprotect,fill,written;UINT oldcp=GetConsoleOutputCP();
    PVOID *slot;PVOID old;unsigned alias;LONG s;
    partialBase=GetModuleHandleW(L"KxBase.dll");
    slot=namedImport(kex,partialBase?"GetProcAddress":"WriteConsoleW");
    check("partial-import-found",slot!=NULL);if(!slot)return;
    SetConsoleOutputCP(CP_UTF8);
    FillConsoleOutputCharacterW(screen,L' ',16,origin,&fill);SetConsoleCursorPosition(screen,origin);
    WriteConsoleW(screen,prefix,2,&written,NULL);cells(screen,reference);
    old=*slot;check("partial-protect",VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&oldprotect));
    if(partialBase){partialResolver=(FARPROC (WINAPI*)(HMODULE,LPCSTR))*slot;*slot=(PVOID)partialGetProc;}
    else *slot=(PVOID)partialWrite;
    partialLimit=2;partialCalls=0;
    for(alias=0;alias<2;++alias){
        FillConsoleOutputCharacterW(screen,L' ',16,origin,&fill);SetConsoleCursorPosition(screen,origin);
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,(PVOID)bytes,sizeof(bytes),NULL,NULL);
        check("partial-byte-count",s==0 && ios.Information==4);
        // Raster-font UTF-8 WriteConsoleW has its own length/rendering defect
        // on this OS. Verify conversion independently of that native defect.
        check("partial-utf8-conversion",partialInputCount==4 && !memcmp(partialInput,whole,sizeof(whole)));
        cells(screen,actual);
        {unsigned j;fprintf(out,"PartialCells=");for(j=0;j<16;++j)fprintf(out,"%04x,",actual[j]);fprintf(out,"\nPartialNativeCells=");for(j=0;j<16;++j)fprintf(out,"%04x,",reference[j]);fprintf(out,"\n");}
    }
    partialLimit=3;
    for(alias=0;alias<2;++alias){
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,(PVOID)bytes,sizeof(bytes),NULL,NULL);
        check("partial-split-surrogate-explicit",s==(LONG)0xc0000161 && ios.Status==s && ios.Information==4);
    }
    SetConsoleOutputCP(932);partialLimit=2;
    FillConsoleOutputCharacterW(screen,L' ',16,origin,&fill);SetConsoleCursorPosition(screen,origin);
    WriteConsoleW(screen,prefix,2,&written,NULL);cells(screen,reference);
    for(alias=0;alias<2;++alias){
        BYTE sjis[]={0x41,0x93,0xfa,0x42};
        FillConsoleOutputCharacterW(screen,L' ',16,origin,&fill);SetConsoleCursorPosition(screen,origin);
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,sjis,sizeof(sjis),NULL,NULL);
        check("partial-dbcs-bytes",s==0 && ios.Information==3);
        check("partial-dbcs-content",cells(screen,actual) && !memcmp(reference,actual,sizeof(actual)));
    }
    *slot=old;VirtualProtect(slot,sizeof(*slot),oldprotect,&oldprotect);
    check("partial-calls",partialCalls==6);SetConsoleOutputCP(oldcp);
    fprintf(out,"PartialCompletion Injection=owned-KexDll-IAT Calls=%lu UTF8Units=2 Bytes=4 DBCSUnits=2 Bytes=3 SplitSurrogate=explicit-error NativeUtf8PartialRendering=unreliable\n",partialCalls);
}
static void child(HANDLE screen,const char *exe,const char *directory,BOOL pipeOutput) {
    STARTUPINFOA si; PROCESS_INFORMATION pi; char command[1024],path[MAX_PATH];
    HANDLE file=NULL,r=NULL,w=NULL; SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
    COORD origin={0,0}; DWORD exitcode=0,wait,read,fill; WCHAR wide[40]; char bytes[80]; BOOL ok;
    sprintf(command,"\"%s\"",exe); sprintf(path,"%s\\zig-output.txt",directory);
    if(pipeOutput) check("create-child-pipe",CreatePipe(&r,&w,&sa,0));
    else file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdError=screen;
    si.hStdOutput=pipeOutput?w:file;
    ZeroMemory(&pi,sizeof(pi));
    ok=CreateProcessA(exe,command,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
    check("zig-redirect-create",ok);
    if(ok) {
        wait=WaitForSingleObject(pi.hProcess,10000);
        if(wait==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,0xdead);WaitForSingleObject(pi.hProcess,5000);}
        GetExitCodeProcess(pi.hProcess,&exitcode);
        check("zig-redirect-exit",wait==WAIT_OBJECT_0 && exitcode==0);
        if(pipeOutput){CloseHandle(w);w=NULL;}
        else SetFilePointer(file,0,NULL,FILE_BEGIN);
        ZeroMemory(bytes,sizeof(bytes));
        check("zig-redirect-bytes",ReadFile(pipeOutput?r:file,bytes,sizeof(bytes),&read,NULL) && read==40 && !memcmp(bytes,"KxNt Zig standard-library console smoke\n",40));
        fprintf(out,"ZigRedirect Pipe=%d Exit=%08lx Bytes=%lu\n",pipeOutput,exitcode,read);
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    }
    if(r)CloseHandle(r);if(w)CloseHandle(w);if(file && file!=INVALID_HANDLE_VALUE)CloseHandle(file);DeleteFileA(path);
    FillConsoleOutputCharacterW(screen,L' ',40,origin,&fill);SetConsoleCursorPosition(screen,origin);
    si.hStdOutput=screen;si.hStdError=screen;ZeroMemory(&pi,sizeof(pi));
    if(debugZig){sprintf(command,"\"C:\\VxKexProbe\\Wow64\\cdb.exe\" -G -logo C:\\VxKexProbe\\KxNtIfeo\\zig-write-%u.log -cf C:\\VxKexProbe\\KxNtIfeo\\zig-write.cdb \"%s\"",++debugInstance,exe);}
    fprintf(out,"ZigConsoleLaunch Debugger=%d ExpectedHandle=%p\n",debugZig,screen);fflush(out);
    ok=CreateProcessA(debugZig?"C:\\VxKexProbe\\Wow64\\cdb.exe":exe,command,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);check("zig-console-create",ok);
    if(ok) {
        wait=WaitForSingleObject(pi.hProcess,10000);
        if(wait==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,0xdead);WaitForSingleObject(pi.hProcess,5000);}
        GetExitCodeProcess(pi.hProcess,&exitcode);
        check("zig-console-exit",wait==WAIT_OBJECT_0 && exitcode==0);
        fprintf(out,"ZigConsoleWait Wait=%08lx Exit=%08lx Debugger=%d\n",wait,exitcode,debugZig);
        ZeroMemory(wide,sizeof(wide));
        check("zig-console-content",ReadConsoleOutputCharacterW(screen,wide,39,origin,&read) && read==39 && !memcmp(wide,L"KxNt Zig standard-library console smoke",39*2));
        fprintf(out,"ZigConsole Exit=%08lx ReadUnits=%lu\n",exitcode,read);
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    }
}
int main(int argc,char **argv) {
    HMODULE kx,kex; PVOID data; HANDLE screen=INVALID_HANDLE_VALUE,extras[128],file,r,w;
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE}; ULONG *prefix; IOS ios,ref; LONG s,t;
    char directory[MAX_PATH],path[MAX_PATH],bytes[8]; DWORD read,oldprotect,fill;
    BYTE *guard; unsigned i,n=0,alias; COORD origin={0,0}; WCHAR before[8],after[8];
    const BYTE utf8[]={0x41,0xe6,0x97,0xa5,0xf0,0x9f,0x98,0x80};
    const WCHAR expected[]={L'A',0x65e5,0xd83d,0xde00}; const BYTE sjis[]={0x41,0x93,0xfa};
    if(argc==99){NtWaitForAlertByThreadId(NULL,NULL);RtlReportSilentProcessExit(NULL,0);NtWriteFile(NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,NULL);}
    if(argc!=4 && argc!=5)return 2;out=fopen(argv[1],"w");if(!out)return 3;
    debugZig=argc==5 && !strcmp(argv[4],"debug-zig");
    profile_bounds();
    kx=LoadLibraryA(argv[2]);kex=GetModuleHandleW(L"KexDll.dll");
    if(!kx || !kex)return 4;
    {char modulepath[MAX_PATH];GetModuleFileNameA(kx,modulepath,sizeof(modulepath));fprintf(out,"KxNtPath=%s\n",modulepath);GetModuleFileNameA(kex,modulepath,sizeof(modulepath));fprintf(out,"KexDllPath=%s\n",modulepath);}
#ifdef KXNT_IFEO_SUITE_PROVIDER_H
    {BYTE *image=(BYTE*)GetModuleHandleW(NULL);IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS*)(image+((IMAGE_DOS_HEADER*)image)->e_lfanew);
     ULONG size=nt->OptionalHeader.SizeOfImage,rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress,j;
     for(j=0;j<nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++j){IMAGE_IMPORT_DESCRIPTOR *d;
      if(!ZigIoRange(size,rva+j*sizeof(IMAGE_IMPORT_DESCRIPTOR),sizeof(IMAGE_IMPORT_DESCRIPTOR)))break;
      d=(IMAGE_IMPORT_DESCRIPTOR*)(image+rva+j*sizeof(IMAGE_IMPORT_DESCRIPTOR));if(!d->Name)break;
      if(ZigIoRange(size,d->Name,1))fprintf(out,"MappedImport Name=%.*s OriginalFirstThunk=%08lx\n",(int)min(size-d->Name,64),image+d->Name,d->OriginalFirstThunk);
     }}
#endif
    adapted[0]=(WRITE_NT)GetProcAddress(kx,"NtWriteFile");adapted[1]=(WRITE_NT)GetProcAddress(kx,"ZwWriteFile");
    native=(WRITE_NT)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtWriteFile");
    query=(QUERY_OBJECT)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject");
    getstatus=(GET_STATUS)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetLastNtStatus");
    seedstatus=(SEED_STATUS)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    if(!adapted[0]||!adapted[1]||!native||!query||!getstatus||!seedstatus)return 5;
    s=((INIT_DATA)GetProcAddress(kex,"KexDataInitialize"))(&data);if(s<0||!data)return 6;prefix=(ULONG*)data;
    if(argc==5 && !debugZig){
        BYTE *image=(BYTE*)GetModuleHandleW(NULL);IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS*)(image+((IMAGE_DOS_HEADER*)image)->e_lfanew);
        IMAGE_SECTION_HEADER *section=IMAGE_FIRST_SECTION(nt);
        for(i=0;i<nt->FileHeader.NumberOfSections;++i)if(!memcmp(section[i].Name,".buildid",8)){VirtualProtect(section[i].Name,8,PAGE_READWRITE,&oldprotect);section[i].Name[0]='X';VirtualProtect(section[i].Name,8,oldprotect,&oldprotect);break;}
        check("unprofiled-content",!ZigNtIoImage(image,nt->OptionalHeader.SizeOfImage));
    }
    else check("profile-selected",ZigNtIoImage((PBYTE)GetModuleHandleW(NULL),((PIMAGE_NT_HEADERS)((PBYTE)GetModuleHandleW(NULL)+((PIMAGE_DOS_HEADER)GetModuleHandleW(NULL))->e_lfanew))->OptionalHeader.SizeOfImage));
    for(i=0;i<128;++i){HANDLE h=CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,CONSOLE_TEXTMODE_BUFFER,NULL);if(h==INVALID_HANDLE_VALUE)break;if(!filetype(h)){screen=h;break;}extras[n++]=h;}
    if(screen==INVALID_HANDLE_VALUE)return 7;
    fprintf(out,"ProcessBits=%u Console=%p\n",(unsigned)(sizeof(void*)*8),screen);
    if(argc==5 && !debugZig){for(alias=0;alias<2;++alias){memset(&ios,0xa5,sizeof(ios));ref=ios;s=adapted[alias](screen,NULL,NULL,NULL,&ios,"X",1,NULL,NULL);t=native(screen,NULL,NULL,NULL,&ref,"X",1,NULL,NULL);check("unprofiled-native",s==t && sameios(&ios,&ref));}stressNative=TRUE;stress(screen);CloseHandle(screen);for(i=0;i<n;++i)CloseHandle(extras[i]);fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;}
    {CONSOLE_FONT_INFOEX font;ZeroMemory(&font,sizeof(font));font.cbSize=sizeof(font);if(GetCurrentConsoleFontEx(screen,FALSE,&font))fprintf(out,"OwnedBufferFont=%ls Family=%u\n",font.FaceName,font.FontFamily);}
    display(screen,437,(const BYTE*)"ASCII",5,L"ASCII",5);
    display(screen,932,sjis,sizeof(sjis),expected,2);
    display(screen,CP_UTF8,utf8,sizeof(utf8),expected,4);
    {union{ULONG_PTR Align;BYTE Bytes[80];}storage;unsigned offset,j;
        for(alias=0;alias<2;++alias)for(offset=0;offset<8;++offset){IOS *p=(IOS*)(storage.Bytes+16+offset);BOOL guards=TRUE;memset(&storage,0xa5,sizeof(storage));s=adapted[alias](screen,NULL,NULL,NULL,p,"U",1,NULL,NULL);for(j=0;j<sizeof(storage.Bytes);++j)if((j<16+offset || j>=16+offset+sizeof(IOS)) && storage.Bytes[j]!=0xa5)guards=FALSE;check("unaligned-console-ios",s==0 && p->Status==0 && p->Information==1 && guards);}}
    partial_cases(kex,screen);
    GetTempPathA(sizeof(directory),directory);sprintf(path,"%skxnt-condrv-%lu.tmp",directory,GetCurrentProcessId());
    file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    check("create-file",file!=INVALID_HANDLE_VALUE);check("create-pipe",CreatePipe(&r,&w,NULL,0));
    for(alias=0;alias<2;++alias){
        for(i=0;i<2;++i){HANDLE h=i?w:file;memset(&ios,0xa5,sizeof(ios));s=adapted[alias](h,NULL,NULL,NULL,&ios,"X",1,NULL,NULL);check("native-file-pipe",s==0 && ios.Status==0 && ios.Information==1);}
        memset(&ios,0xa5,sizeof(ios));ref=ios;s=adapted[alias]((HANDLE)1,NULL,NULL,NULL,&ios,"X",1,NULL,NULL);t=native((HANDLE)1,NULL,NULL,NULL,&ref,"X",1,NULL,NULL);check("native-invalid",s==t && sameios(&ios,&ref));
        memset(&ios,0xa5,sizeof(ios));s=adapted[alias](screen,NULL,NULL,NULL,&ios,(PVOID)1,0,NULL,NULL);check("zero-length",s==0 && ios.Information==0);
        FillConsoleOutputCharacterW(screen,L'Q',8,origin,&fill);ReadConsoleOutputCharacterW(screen,before,8,origin,&read);
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,(PVOID)1,1,NULL,NULL);check("bad-input",s==(LONG)0xc0000005);
        s=adapted[alias](screen,NULL,NULL,NULL,(IOS*)1,"X",1,NULL,NULL);check("bad-output",s==(LONG)0xc0000005);
        s=adapted[alias](screen,NULL,NULL,NULL,NULL,"X",1,NULL,NULL);check("null-output",s==(LONG)0xc0000005);
        s=adapted[alias](screen,(HANDLE)1,NULL,NULL,&ios,"X",1,NULL,NULL);check("async-console-explicit",s==(LONG)0xc00000bb);
        {LONGLONG offset=5;s=adapted[alias](screen,NULL,NULL,NULL,&ios,"X",1,&offset,NULL);check("invalid-console-offset",s==(LONG)0xc000000d);}
        ReadConsoleOutputCharacterW(screen,after,8,origin,&read);check("errors-no-console-write",!memcmp(before,after,sizeof(before)));
        prefix[2]=1;memset(&ios,0xa5,sizeof(ios));ref=ios;s=adapted[alias](screen,NULL,NULL,NULL,&ios,"X",1,NULL,NULL);t=native(screen,NULL,NULL,NULL,&ref,"X",1,NULL,NULL);prefix[2]=0;check("profile-disabled-native",s==t && sameios(&ios,&ref));
        guard=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);VirtualProtect(guard,4096,PAGE_READONLY,&oldprotect);s=adapted[alias](screen,NULL,NULL,NULL,(IOS*)guard,"X",1,NULL,NULL);check("readonly-ios",s==(LONG)0xc0000005);VirtualFree(guard,0,MEM_RELEASE);
        guard=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);VirtualProtect(guard+4096,4096,PAGE_NOACCESS,&oldprotect);
        s=adapted[alias](screen,NULL,NULL,NULL,&ios,guard+4095,2,NULL,NULL);check("input-guard",s==(LONG)0xc0000005);
        s=adapted[alias](screen,NULL,NULL,NULL,(IOS*)(guard+4096-sizeof(IOS)+1),"X",1,NULL,NULL);check("output-guard",s==(LONG)0xc0000005);VirtualFree(guard,0,MEM_RELEASE);
        {UINT cp=GetConsoleOutputCP();BYTE invalid[]={0xf0,0x80,0x80,0x80};SetConsoleOutputCP(CP_UTF8);s=adapted[alias](screen,NULL,NULL,NULL,&ios,invalid,4,NULL,NULL);SetConsoleOutputCP(cp);check("invalid-utf8-explicit",s==(LONG)0xc0000161);}
    }
    SetFilePointer(file,0,NULL,FILE_BEGIN);ZeroMemory(bytes,sizeof(bytes));check("file-content",ReadFile(file,bytes,sizeof(bytes),&read,NULL) && read==2 && !memcmp(bytes,"XX",2));
    ZeroMemory(bytes,sizeof(bytes));check("pipe-content",ReadFile(r,bytes,2,&read,NULL) && read==2 && !memcmp(bytes,"XX",2));
    collision(file);
    CloseHandle(file);DeleteFileA(path);CloseHandle(r);CloseHandle(w);
    write_only();
    native_routes(directory);
    stress(screen);
    child(screen,argv[3],directory,FALSE);child(screen,argv[3],directory,TRUE);
    {
        char basepath[MAX_PATH];HMODULE base;BOOL (WINAPI *modefn)(HANDLE,DWORD);
        strcpy(basepath,argv[2]);strcpy(strrchr(basepath,'\\')+1,"KxBase.dll");base=LoadLibraryA(basepath);
        check("vt-module-load",base!=NULL);
        if(base){modefn=(BOOL (WINAPI*)(HANDLE,DWORD))GetProcAddress(base,"SetConsoleMode");check("vt-mode-export",modefn!=NULL);if(modefn){DWORD mode;GetConsoleMode(screen,&mode);check("vt-mode-enable",modefn(screen,mode|4));FillConsoleOutputCharacterW(screen,L' ',8,origin,&fill);SetConsoleCursorPosition(screen,origin);s=adapted[0](screen,NULL,NULL,NULL,&ios,"\x1b[31mR\x1b[0m",10,NULL,NULL);ReadConsoleOutputCharacterW(screen,after,1,origin,&read);check("vt-output",s==0 && ios.Information==10 && after[0]==L'R');modefn(screen,mode);}}
    }
    CloseHandle(screen);for(i=0;i<n;++i)CloseHandle(extras[i]);
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
