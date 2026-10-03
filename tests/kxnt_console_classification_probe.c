#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *QUERY_OBJECT)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef struct { LONG Status; ULONG_PTR Information; } IOS;
typedef LONG (WINAPI *WRITE_NT)(HANDLE,HANDLE,PVOID,PVOID,IOS*,PVOID,ULONG,PVOID,PULONG);
typedef BOOL (WINAPI *VERIFY_CONSOLE)(HANDLE);
typedef struct { USHORT Length,MaximumLength; PWSTR Buffer; } USTRING;
static FILE *out;
static QUERY_OBJECT query;
static WRITE_NT ntwrite;
static VERIFY_CONSOLE verify;

static void classify(const char *name,HANDLE h)
{
    union { ULONG_PTR Align; BYTE Bytes[4096]; } info;
    ULONG needed=0; LONG status; DWORD mode=0,modeError,type,typeError;
    BOOL modeOk,verified=FALSE; char typeName[80]; unsigned i,n=0;
    ZeroMemory(&info,sizeof(info));
    status=query(h,2,info.Bytes,sizeof(info.Bytes),&needed);
    typeName[0]=0;
    if(status>=0) {
        USTRING *s=(USTRING*)info.Bytes;
        n=s->Length/sizeof(WCHAR); if(n>79)n=79;
        for(i=0;i<n;i++)typeName[i]=(char)s->Buffer[i]; typeName[n]=0;
    }
    SetLastError(0); modeOk=GetConsoleMode(h,&mode); modeError=GetLastError();
    SetLastError(0); type=GetFileType(h); typeError=GetLastError();
    if(verify)verified=verify(h);
    fprintf(out,"Class=%s Handle=%p ObjectStatus=%08lx ObjectType=%s Needed=%lu ModeOk=%d Mode=%lx ModeError=%lu FileType=%lu TypeError=%lu Verified=%d\n",
        name,h,(unsigned long)status,typeName,needed,modeOk,mode,modeError,type,typeError,verified);
}

static void write_native(const char *name,HANDLE h)
{
    IOS ios; LONG s; char byte='N';
    ios.Status=(LONG)0xdeadbeef; ios.Information=(ULONG_PTR)0xdeadbeef;
    s=ntwrite(h,NULL,NULL,NULL,&ios,&byte,1,NULL,NULL);
    fprintf(out,"NativeWrite=%s Status=%08lx IoStatus=%08lx Bytes=%Iu\n",name,(unsigned long)s,(unsigned long)ios.Status,ios.Information);
}

static void write_console_native_if_owned(const char *name,HANDLE h,HANDLE *owned,unsigned count)
{
    BYTE info[4096]; ULONG needed; LONG s; unsigned i;
    s=query(h,2,info,sizeof(info),&needed);
    if(s>=0) {
        for(i=0;i<count;i++)if(((ULONG_PTR)h & ~(ULONG_PTR)3)==(ULONG_PTR)owned[i])break;
        if(i==count){fprintf(out,"NativeWrite=%s Skipped=kernel-alias-not-owned\n",name);return;}
    }
    write_native(name,h);
}

int main(int argc,char **argv)
{
    HANDLE rw,wo,input,file,readPipe,writePipe,files[128],screens[128];
    WCHAR directory[MAX_PATH],path[MAX_PATH]; DWORD written,error; BOOL ok;
    unsigned i,count=0,screenCount=0; ULONG_PTR alias; char bytes[16];
    if(argc!=2)return 2;
    out=fopen(argv[1],"w"); if(!out)return 3;
    query=(QUERY_OBJECT)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject");
    ntwrite=(WRITE_NT)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtWriteFile");
    verify=(VERIFY_CONSOLE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"VerifyConsoleIoHandle");
    fprintf(out,"ProcessBits=%u VerifyExport=%d KexDllLoaded=%d\n",(unsigned)(sizeof(void*)*8),verify!=NULL,GetModuleHandleW(L"KexDll.dll")!=NULL);
    if(!query || !ntwrite)return 4;
    ok=AllocConsole(); error=GetLastError();
    fprintf(out,"AllocConsole=%d Error=%lu\n",ok,error);
    classify("stdout",GetStdHandle(STD_OUTPUT_HANDLE));
    classify("stdin",GetStdHandle(STD_INPUT_HANDLE));
    rw=CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CONSOLE_TEXTMODE_BUFFER,NULL);
    wo=CreateConsoleScreenBuffer(GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CONSOLE_TEXTMODE_BUFFER,NULL);
    input=CreateFileW(L"CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(rw==INVALID_HANDLE_VALUE || wo==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE){fprintf(out,"ConsoleSetupError=%lu\n",GetLastError());return 5;}
    classify("screen-rw",rw); classify("screen-write-only",wo); classify("input",input);
    GetTempPathW(MAX_PATH,directory); GetTempFileNameW(directory,L"kxc",0,path);
    file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if(file==INVALID_HANDLE_VALUE || !CreatePipe(&readPipe,&writePipe,NULL,0))return 6;
    classify("file",file); classify("pipe-write",writePipe); classify("invalid",(HANDLE)(ULONG_PTR)0x7ffffffc);
    classify("null",NULL); classify("file-tag1",(HANDLE)((ULONG_PTR)file|1));
    classify("file-tag3",(HANDLE)((ULONG_PTR)file|3));
    // All NT writes are directed at our disposable resources. Find a live
    // kernel handle numerically equal to the private console handle minus tags.
    alias=(ULONG_PTR)rw & ~(ULONG_PTR)3;
    fprintf(out,"ConsoleKernelAliasTarget=%p\n",(void*)alias);
    for(i=0;i<128;i++) {
        files[count]=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY,NULL);
        if(files[count]==INVALID_HANDLE_VALUE)break;
        if((ULONG_PTR)files[count]==alias){classify("kernel-alias",files[count]);break;}
        count++;
    }
    if(i<128 && files[count]!=INVALID_HANDLE_VALUE)count++;
    fprintf(out,"KernelAliasFound=%d Opened=%u\n",i<128 && count && (ULONG_PTR)files[count-1]==alias,count);
    write_console_native_if_owned("screen-rw",rw,files,count);
    write_console_native_if_owned("screen-write-only",wo,files,count);
    write_native("file-tag3",(HANDLE)((ULONG_PTR)file|3));
    write_native("pipe-write",writePipe);
    // Exercise both handle namespaces with the SAME numeric value. This is
    // not a malformed handle: one live file and one live console own the value.
    for(i=0;i<128;i++) {
        screens[screenCount]=CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CONSOLE_TEXTMODE_BUFFER,NULL);
        if(screens[screenCount]==INVALID_HANDLE_VALUE)break;
        screenCount++;
        if(screens[screenCount-1]==(HANDLE)((ULONG_PTR)file|3)) {
            classify("console-file-numeric-collision",screens[screenCount-1]);
            write_native("console-file-numeric-collision",screens[screenCount-1]);
            written=0;ok=WriteConsoleA(screens[screenCount-1],"C",1,&written,NULL);
            fprintf(out,"ConsoleWrite=console-file-numeric-collision Ok=%d Bytes=%lu Error=%lu\n",ok,written,GetLastError());
            break;
        }
    }
    fprintf(out,"ConsoleFileCollisionFound=%d Created=%u\n",screenCount && screens[screenCount-1]==(HANDLE)((ULONG_PTR)file|3),screenCount);
    ZeroMemory(bytes,sizeof(bytes)); SetFilePointer(file,0,NULL,FILE_BEGIN); ReadFile(file,bytes,15,&written,NULL);
    fprintf(out,"DisposableFileBytes=%lu Text=%s\n",written,bytes);
    written=0; ok=WriteConsoleA(rw,"A",1,&written,NULL);
    fprintf(out,"ConsoleWrite=screen-rw Ok=%d Bytes=%lu Error=%lu\n",ok,written,GetLastError());
    written=0; ok=WriteConsoleA(wo,"B",1,&written,NULL);
    fprintf(out,"ConsoleWrite=screen-write-only Ok=%d Bytes=%lu Error=%lu\n",ok,written,GetLastError());
    for(i=0;i<count;i++)CloseHandle(files[i]);
    for(i=0;i<screenCount;i++)CloseHandle(screens[i]);
    CloseHandle(file);CloseHandle(readPipe);CloseHandle(writePipe);DeleteFileW(path);
    CloseHandle(rw);CloseHandle(wo);CloseHandle(input);
    fprintf(out,"Result=MEASURED\n");fclose(out);return 0;
}
