#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { union { LONG status; PVOID pointer; }; ULONG_PTR information; } IO_BLOCK;
typedef LONG (WINAPI *SET_FN)(HANDLE,IO_BLOCK*,PVOID,ULONG,ULONG);
typedef struct { ULONG flags; HANDLE root; ULONG bytes; WCHAR name[1]; } RENAME;
static SET_FN setFile, nativeSet;
static FILE *out;
static unsigned cases,failures;
static WCHAR directory[MAX_PATH];
static const DWORD marker=0x76543210;
static int nativeMode;

static void result(const char *name,int pass,LONG status)
{
    ++cases; if(!pass)++failures;
    fprintf(out,"%s Status=%08lx %s\n",name,(ULONG)status,pass?"PASS":"FAIL");
}
static void path(WCHAR *destination,const WCHAR *name)
{ swprintf(destination,MAX_PATH,L"%s\\%s",directory,name); }
static void clean(const WCHAR *p)
{ SetFileAttributesW(p,FILE_ATTRIBUTE_NORMAL);DeleteFileW(p); }
static int create(const WCHAR *p,char data)
{
    DWORD written; HANDLE h=CreateFileW(p,GENERIC_WRITE,7,NULL,CREATE_ALWAYS,0,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    if(!WriteFile(h,&data,1,&written,NULL)||written!=1){CloseHandle(h);return 0;}
    return CloseHandle(h)!=0;
}
static int content(const WCHAR *p)
{
    char data=0;DWORD read;HANDLE h=CreateFileW(p,GENERIC_READ,7,NULL,OPEN_EXISTING,0,NULL);
    if(h==INVALID_HANDLE_VALUE)return GetFileAttributesW(p)==INVALID_FILE_ATTRIBUTES?-1:-2;
    if(!ReadFile(h,&data,1,&read,NULL)||read!=1)data=0;
    CloseHandle(h);return data;
}
static RENAME *renameInfo(const WCHAR *name,HANDLE root,ULONG flags,ULONG *length)
{
    ULONG bytes=(ULONG)(wcslen(name)*sizeof(WCHAR));
    RENAME *info;
    *length=(ULONG)offsetof(RENAME,name)+bytes;
    if(*length<sizeof(RENAME))*length=sizeof(RENAME);
    info=(RENAME*)calloc(1,*length);
    if(info){info->flags=flags;info->root=root;info->bytes=bytes;memcpy(info->name,name,bytes);}
    return info;
}
static LONG invoke(SET_FN fn,HANDLE h,IO_BLOCK *io,void *data,ULONG bytes,ULONG cls,int *error)
{
    LONG status;
    SetLastError(marker);
    __try { status=fn(h,io,data,bytes,cls); }
    __except(EXCEPTION_EXECUTE_HANDLER){ status=GetExceptionCode(); }
    if(GetLastError()!=marker)*error=1;
    return status;
}
static void renameCase(const char *label,ULONG flags,int exists,int readonly,int relative,int overlapped,int noDelete)
{
    WCHAR source[2][MAX_PATH],target[2][MAX_PATH],nativeName[MAX_PATH+8];
    const WCHAR *targets[2]={L"\x65e5\x672c-a.txt",L"\x65e5\x672c-b.txt"};
    HANDLE root=NULL,h;
    LONG status[2]; IO_BLOCK io[2];
    int error=0,src[2],dst[2],i;
    if(relative)root=CreateFileW(directory,FILE_TRAVERSE|FILE_READ_ATTRIBUTES,7,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    for(i=0;i<2;++i){
        RENAME *info,*saved;ULONG length;
        path(source[i],i?L"source-b.txt":L"source-a.txt");path(target[i],targets[i]);
        clean(source[i]);clean(target[i]);
        if(!create(source[i],'S')||(exists&&!create(target[i],'T')))error=1;
        if(readonly&&!SetFileAttributesW(target[i],FILE_ATTRIBUTE_READONLY))error=1;
        h=CreateFileW(source[i],GENERIC_READ|(noDelete?0:DELETE),7,NULL,OPEN_EXISTING,overlapped?FILE_FLAG_OVERLAPPED:0,NULL);
        if(h==INVALID_HANDLE_VALUE)error=1;
        if(relative)wcscpy(nativeName,targets[i]);else swprintf(nativeName,MAX_PATH+8,L"\\??\\%s",target[i]);
        info=renameInfo(nativeName,root,flags,&length);saved=(RENAME*)malloc(length);
        if(!info||!saved){result(label,0,0xc0000017);return;}
        memcpy(saved,info,length);memset(&io[i],0xa5,sizeof(io[i]));
        status[i]=invoke(i?nativeSet:setFile,h,&io[i],info,length,i?10:65,&error);
        if(memcmp(saved,info,length))error=1;
        free(saved);free(info);CloseHandle(h);
        src[i]=content(source[i]);dst[i]=content(target[i]);
    }
    if(root&&root!=INVALID_HANDLE_VALUE)CloseHandle(root);
    result(label,!error&&status[0]==status[1]&&memcmp(io,io+1,sizeof(IO_BLOCK))==0&&src[0]==src[1]&&dst[0]==dst[1]&&
        (ULONG)status[0]==(noDelete||readonly?0xc0000022UL:exists&&!flags?0xc0000035UL:0)&&
        src[0]==(status[0]==0?-1:'S')&&dst[0]==(status[0]==0?'S':exists?'T':-1),status[0]);
    for(i=0;i<2;++i){clean(source[i]);clean(target[i]);}
}
static void deleteCase(const char *label,ULONG flags,int readonly,int noDelete,int overlapped)
{
    WCHAR p[2][MAX_PATH];LONG status[2];IO_BLOCK io[2];int state[2],error=0,i;
    for(i=0;i<2;++i){
        HANDLE h;ULONG bits=flags;BOOLEAN old=(BOOLEAN)flags;
        path(p[i],i?L"delete-b.txt":L"delete-a.txt");clean(p[i]);if(!create(p[i],'D'))error=1;
        if(readonly&&!SetFileAttributesW(p[i],FILE_ATTRIBUTE_READONLY))error=1;
        h=CreateFileW(p[i],GENERIC_READ|(noDelete?0:DELETE),7,NULL,OPEN_EXISTING,overlapped?FILE_FLAG_OVERLAPPED:0,NULL);
        memset(io+i,0xa5,sizeof(IO_BLOCK));
        status[i]=invoke(i?nativeSet:setFile,h,io+i,i?(void*)&old:(void*)&bits,i?1:4,i?13:64,&error);
        if(bits!=flags)error=1;
        CloseHandle(h);state[i]=content(p[i]);
    }
    result(label,!error&&status[0]==status[1]&&memcmp(io,io+1,sizeof(IO_BLOCK))==0&&state[0]==state[1]&&
        (ULONG)status[0]==(noDelete?0xc0000022UL:readonly?0xc0000121UL:0)&&
        state[0]==(status[0]==0&&flags?-1:'D'),status[0]);
    clean(p[0]);clean(p[1]);
}
static void invalidCase(const char *label,HANDLE h,void *buffer,ULONG length,ULONG cls,ULONG expected)
{
    IO_BLOCK io,saved;int error=0;LONG status;
    memset(&io,0xa5,sizeof(io));saved=io;
    status=invoke(setFile,h,&io,buffer,length,cls,&error);
    result(label,!error&&(ULONG)status==expected&&memcmp(&saved,&io,sizeof(io))==0,status);
}
static void validationCases(void)
{
    WCHAR p[MAX_PATH],t[MAX_PATH];HANDLE h;ULONG bits,length;RENAME *info;
    SYSTEM_INFO si;BYTE *guard;DWORD protect;int i;
    ULONG extra[]={2,3,4,8,16,64,0x80000000};
    char label[80];
    path(p,L"invalid-source.txt");path(t,L"invalid-target.txt");clean(p);clean(t);
    create(p,'S');create(t,'T');h=CreateFileW(p,GENERIC_READ|DELETE,7,NULL,OPEN_EXISTING,0,NULL);
    info=renameInfo(L"invalid-target.txt",NULL,0,&length);
    for(i=0;i<7;++i){
        bits=extra[i];sprintf(label,"delete-unsupported-%08lx",bits);invalidCase(label,h,&bits,4,64,0xc00000bb);
        info->flags=bits;sprintf(label,"rename-unsupported-%08lx",bits);invalidCase(label,h,info,length,65,0xc00000bb);
    }
    bits=0;info->flags=0;
    invalidCase("delete-short",h,&bits,3,64,0xc0000004);
    invalidCase("delete-long",h,&bits,5,64,0xc0000004);
    invalidCase("delete-null",h,NULL,4,64,0xc0000005);
    invalidCase("rename-short",h,info,sizeof(RENAME)-1,65,0xc0000004);
    invalidCase("rename-null",h,NULL,length,65,0xc0000005);
    info->bytes=0;invalidCase("rename-empty",h,info,length,65,0xc000000d);
    info->bytes=3;invalidCase("rename-odd",h,info,length,65,0xc000000d);
    info->bytes=length;invalidCase("rename-truncated",h,info,length,65,0xc000000d);
    info->bytes=0xffffffff;invalidCase("rename-length-wrap",h,info,0xffffffff,65,0xc000000d);
    info->bytes=65536;invalidCase("rename-name-too-long",h,info,0xffffffff,65,0xc0000106);
    GetSystemInfo(&si);guard=(BYTE*)VirtualAlloc(NULL,si.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!guard){result("guard-allocation",0,0xc0000017);return;}
    VirtualProtect(guard+si.dwPageSize,si.dwPageSize,PAGE_NOACCESS,&protect);
    invalidCase("delete-guard",h,guard+si.dwPageSize-3,4,64,0xc0000005);
    invalidCase("rename-header-guard",h,guard+si.dwPageSize-(DWORD)offsetof(RENAME,name)+1,length,65,0xc0000005);
    memset(info,0,length);info->bytes=4;
    memcpy(guard+si.dwPageSize-(DWORD)offsetof(RENAME,name)-2,info,offsetof(RENAME,name));
    invalidCase("rename-name-guard",h,guard+si.dwPageSize-(DWORD)offsetof(RENAME,name)-2,length,65,0xc0000005);
    VirtualFree(guard,0,MEM_RELEASE);free(info);CloseHandle(h);
    result("rejected-inputs-files-unchanged",content(p)=='S'&&content(t)=='T',0);
    clean(p);clean(t);
}
static void ordinaryAndStress(void)
{
    WCHAR p[MAX_PATH],q[MAX_PATH];HANDLE h;LARGE_INTEGER position;
    IO_BLOCK io[2];LONG s[2];int error=0,i;DWORD before,after;
    RENAME *a,*b;ULONG al,bl;
    path(p,L"stress-a.txt");path(q,L"stress-b.txt");clean(p);clean(q);create(p,'S');
    h=CreateFileW(p,GENERIC_READ|DELETE,7,NULL,OPEN_EXISTING,0,NULL);
    position.QuadPart=0;
    for(i=0;i<2;++i){memset(io+i,0xa5,sizeof(IO_BLOCK));s[i]=invoke(i?nativeSet:setFile,h,io+i,&position,sizeof(position),14,&error);}
    result("ordinary-position-forward",!error&&s[0]==s[1]&&!memcmp(io,io+1,sizeof(IO_BLOCK)),s[0]);
    for(i=0;i<2;++i){memset(io+i,0xa5,sizeof(IO_BLOCK));s[i]=invoke(i?nativeSet:setFile,(HANDLE)(ULONG_PTR)0x123456,io+i,&position,sizeof(position),14,&error);}
    result("ordinary-invalid-forward",!error&&s[0]==s[1]&&!memcmp(io,io+1,sizeof(IO_BLOCK)),s[0]);
    a=renameInfo(L"stress-a.txt",NULL,0,&al);b=renameInfo(L"stress-b.txt",NULL,0,&bl);
    {
        ULONG flags=0;
        LONG disposition=nativeSet(h,io,&flags,sizeof(flags),64);
        LONG rename=nativeSet(h,io,a,al,65);
        fprintf(out,"Diagnostic NativeDispositionEx=%08lx NativeRenameEx=%08lx\n",(ULONG)disposition,(ULONG)rename);
    }
    // Warm one successful allocation and syscall before resource measurement.
    s[0]=invoke(setFile,h,io,b,bl,65,&error);
    // Console/IME startup is asynchronous on NT 6.0. Separate that known
    // initialization from the following fixed set of measured rename calls.
    Sleep(2000);
    GetProcessHandleCount(GetCurrentProcess(),&before);
    for(i=0;i<1000;++i){RENAME *info=(i&1)?b:a;ULONG len=(i&1)?bl:al;if(invoke(setFile,h,io,info,len,65,&error)!=0)error=1;}
    GetProcessHandleCount(GetCurrentProcess(),&after);
    result("rename-repeat-1000",!error&&s[0]==0&&before==after,0);
    fprintf(out,"RepeatCalls=1000 HandleDelta=%ld\n",(LONG)(after-before));
    free(a);free(b);CloseHandle(h);
    result("rename-repeat-final-content",content(q)=='S'&&content(p)==-1,0);
    clean(p);clean(q);
}
static void readonlyBoundary(void)
{
    SYSTEM_INFO si;BYTE *memory;DWORD protect;RENAME *info;IO_BLOCK io;
    WCHAR p[MAX_PATH],q[MAX_PATH];HANDLE h;int error=0;LONG status;
    BYTE saved[sizeof(RENAME)];ULONG bits=0;
    GetSystemInfo(&si);
    memory=(BYTE*)VirtualAlloc(NULL,si.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!memory){result("boundary-allocation",0,0xc0000017);return;}
    VirtualProtect(memory+si.dwPageSize,si.dwPageSize,PAGE_NOACCESS,&protect);
    info=(RENAME*)(memory+si.dwPageSize-sizeof(RENAME));memset(info,0,sizeof(RENAME));
    info->bytes=2;info->name[0]=L'x';memcpy(saved,info,sizeof(RENAME));
    VirtualProtect(memory,si.dwPageSize,PAGE_READONLY,&protect);
    path(p,L"boundary-source.txt");path(q,L"x");clean(p);clean(q);create(p,'S');
    h=CreateFileW(p,GENERIC_READ|DELETE,7,NULL,OPEN_EXISTING,0,NULL);
    // A guarded, read-only input with a huge advertised trailing length must
    // neither allocate that length nor read past the captured file name.
    status=invoke(setFile,h,&io,info,0xffffffff,65,&error);
    CloseHandle(h);
    result("rename-readonly-huge-trailing-buffer",!error&&status==0&&!memcmp(saved,info,sizeof(RENAME))&&content(p)==-1&&content(q)=='S',status);
    clean(q);create(p,'S');h=CreateFileW(p,GENERIC_READ|DELETE,7,NULL,OPEN_EXISTING,0,NULL);
    status=invoke(setFile,h,(IO_BLOCK*)(memory+si.dwPageSize),&bits,4,64,&error);
    result("delete-output-guard",!error&&(ULONG)status==0xc0000005,status);
    CloseHandle(h);
    result("output-guard-file-unchanged",content(p)=='S',0);
    clean(p);VirtualFree(memory,0,MEM_RELEASE);
}
int main(int argc,char **argv)
{
    HMODULE module;WCHAR temp[MAX_PATH];int which;
    if(argc!=3 && argc!=4)return 2;
    nativeMode=argc==4 && strcmp(argv[3],"Native")==0;
    out=fopen(argv[2],"w");if(!out)return 3;
    module=LoadLibraryA(argv[1]);if(!module)return 4;
    nativeSet=(SET_FN)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtSetInformationFile");
    if(!GetTempPathW(MAX_PATH,temp))return 5;
    swprintf(directory,MAX_PATH,L"%sKxNtFile-%lu",temp,GetCurrentProcessId());
    if(!CreateDirectoryW(directory,NULL))return 6;
    fprintf(out,"ProcessBits=%u\n",(unsigned)(sizeof(void*)*8));
    fprintf(out,"Diagnostic RenameHeader=%u RenameSize=%u\n",(unsigned)offsetof(RENAME,name),(unsigned)sizeof(RENAME));
    for(which=0;which<2;++which){
        setFile=(SET_FN)GetProcAddress(module,which?"ZwSetInformationFile":"NtSetInformationFile");
        if(!setFile)return 7;
        fprintf(out,"Entry=%s\n",which?"ZwSetInformationFile":"NtSetInformationFile");
        renameCase("rename-absolute",0,0,0,0,0,0);
        renameCase("rename-collision",0,1,0,0,0,0);
        renameCase("rename-replace",1,1,0,0,0,0);
        renameCase("rename-readonly-target",1,1,1,0,0,0);
        renameCase("rename-root-relative",0,0,0,1,0,0);
        renameCase("rename-root-replace",1,1,0,1,0,0);
        renameCase("rename-overlapped-handle",0,0,0,0,1,0);
        renameCase("rename-no-delete-access",0,0,0,0,0,1);
        deleteCase("delete-clear",0,0,0,0);
        deleteCase("delete-mark",1,0,0,0);
        deleteCase("delete-readonly",1,1,0,0);
        deleteCase("delete-no-delete-access",1,0,1,0);
        deleteCase("delete-overlapped-handle",1,0,0,1);
        if(!nativeMode){validationCases();readonlyBoundary();}
        ordinaryAndStress();
    }
    RemoveDirectoryW(directory);FreeLibrary(module);
    fprintf(out,"FileInformationCases=%u Failures=%u\nResult=%s\n",cases,failures,failures?"FAIL":"PASS");
    fclose(out);return failures?1:0;
}
