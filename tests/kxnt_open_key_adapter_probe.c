#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef struct {USHORT Length,Maximum;PWSTR Buffer;} STRING;
typedef struct {ULONG Length;HANDLE Root;STRING *Name;ULONG Attributes;PVOID Security,Qos;} ATTR;
#ifdef KXNT_IFEO_IMPORT
__declspec(dllimport) LONG WINAPI NtOpenKeyEx(PHANDLE,ACCESS_MASK,ATTR*,ULONG);
#ifndef _WIN64
#pragma comment(linker,"/alternatename:__imp__NtOpenKeyEx@16=__imp__NtOpenKeyEx")
#endif
#endif
typedef LONG (WINAPI *OPEN)(PHANDLE,ACCESS_MASK,ATTR*);
typedef LONG (WINAPI *OPENEX)(PHANDLE,ACCESS_MASK,ATTR*,ULONG);
typedef LONG (WINAPI *QUERY)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *GETSTATUS)(void);
typedef VOID (WINAPI *SEED)(LONG);
typedef struct {LONG Status,QueryStatus,ObjectStatus;DWORD Error;LONG TLS;BOOL Changed;ULONG Access;WCHAR Name[256];} RESULT;
static FILE *out;static unsigned failures;static OPEN old;static OPENEX ex;static QUERY query,object;static GETSTATUS getstatus;static SEED seed;
static RESULT measure(BOOL extended,PHANDLE output,ACCESS_MASK access,ATTR *attr,ULONG options){
    RESULT r;ULONG needed;union {ULONG_PTR align;BYTE bytes[1024];} data;HANDLE h=NULL;
    ZeroMemory(&r,sizeof(r));seed((LONG)0xc0000022);SetLastError(0x13579bdf);
    r.Status=extended?ex(output,access,attr,options):old(output,access,attr);
    r.Error=GetLastError();r.TLS=getstatus();
    if(output){memcpy(&h,output,sizeof(h));r.Changed=h!=(HANDLE)(ULONG_PTR)0x12345678;}
    if(r.Status>=0){
        r.QueryStatus=query(h,3,data.bytes,sizeof(data.bytes),&needed);
        if(r.QueryStatus>=0){ULONG bytes=*(ULONG*)data.bytes;if(bytes>=sizeof(r.Name)){++failures;}else memcpy(r.Name,data.bytes+4,bytes);}
        r.ObjectStatus=object(h,0,data.bytes,sizeof(data.bytes),&needed);if(r.ObjectStatus>=0)r.Access=*(ULONG*)(data.bytes+4);
        CloseHandle(h);
    }
    if(r.Error!=0x13579bdf || r.TLS!=(LONG)0xc0000022)++failures;
    return r;
}
int main(int argc,char **argv){
    HMODULE native=GetModuleHandleW(L"ntdll.dll"),provider,implementation;char path[MAX_PATH];HANDLE root=NULL,h;
    STRING name;ATTR a;RESULT before,after;BYTE unaligned[sizeof(ATTR)+16];PVOID guard,readonly;
    ACCESS_MASK access[]={KEY_READ,0,MAXIMUM_ALLOWED,KEY_READ|KEY_WOW64_32KEY,KEY_READ|KEY_WOW64_64KEY};
    ULONG options[]={1,4,8,12,16,24,32,0xffffffff};unsigned i,j,k;DWORD protect,startCount,endCount;LONG s;BOOL nativePresent;
    if(argc!=3)return 2;out=fopen(argv[2],"w");if(!out)return 3;
#ifdef KXNT_IFEO_IMPORT
    /* This image imports ntdll!NtOpenKeyEx. AVRF must rewrite it before main;
       no explicit LoadLibrary may conceal an early-loader failure. */
    provider=GetModuleHandleW(L"KxNt.dll");if(!provider || !GetModuleHandleW(L"KexDll.dll"))return 4;
#else
    provider=LoadLibraryA(argv[1]);if(!provider)return 4;
#endif
    old=(OPEN)GetProcAddress(native,"NtOpenKey");ex=(OPENEX)GetProcAddress(provider,"NtOpenKeyEx");query=(QUERY)GetProcAddress(native,"NtQueryKey");object=(QUERY)GetProcAddress(native,"NtQueryObject");getstatus=(GETSTATUS)GetProcAddress(native,"RtlGetLastNtStatus");seed=(SEED)GetProcAddress(native,"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    if(!old || !ex || !query || !object || !getstatus || !seed)return 5;
#ifdef KXNT_IFEO_IMPORT
    fprintf(out,"StaticImportEqual=%d EarlyKexDllLoaded=1\n",(PVOID)NtOpenKeyEx==(PVOID)ex);
    if((PVOID)NtOpenKeyEx!=(PVOID)ex)return 10;
    ex=NtOpenKeyEx;
#endif
    nativePresent=GetProcAddress(native,"NtOpenKeyEx")!=NULL;
    GetModuleFileNameA(provider,path,sizeof(path));fprintf(out,"Provider=%s\n",path);
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)ex,&implementation))return 6;
    GetModuleFileNameA(implementation,path,sizeof(path));fprintf(out,"Implementation=%s NativePresent=%d AliasEqual=%d\n",path,nativePresent,(PVOID)ex==(PVOID)GetProcAddress(provider,"ZwOpenKeyEx"));
    if((PVOID)ex!=(PVOID)GetProcAddress(provider,"ZwOpenKeyEx"))++failures;
    name.Buffer=L"\\Registry\\Machine\\SYSTEM";name.Length=(USHORT)(wcslen(name.Buffer)*2);name.Maximum=name.Length;
    ZeroMemory(&a,sizeof(a));a.Length=sizeof(a);a.Name=&name;a.Attributes=0x40;
    if(old(&root,KEY_READ,&a)<0)return 7;
    guard=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);readonly=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!guard || !readonly)return 8;
    *(HANDLE*)readonly=(HANDLE)(ULONG_PTR)0x12345678;if(!VirtualProtect(readonly,4096,PAGE_READONLY,&protect))return 9;
    for(i=0;i<14;++i)for(j=0;j<2;++j)for(k=0;k<5;++k){
        ATTR *input=&a;PHANDLE output=&h;
        const WCHAR *text=i==1?L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet":i==2?L"\\Registry\\Machine\\SYSTEM\\VxKexMissing_69133742":i==3?L"CurrentControlSet":i==4?L"":L"\\Registry\\Machine\\SYSTEM";
        name.Buffer=(PWSTR)text;name.Length=(USHORT)(wcslen(text)*2);name.Maximum=name.Length;
        ZeroMemory(&a,sizeof(a));a.Length=sizeof(a);a.Root=(i==3 || i==4)?root:NULL;a.Name=&name;a.Attributes=0x40|(j?0x100:0);
        if(i==5)input=NULL;if(i==6)a.Length=0;if(i==7)a.Name=NULL;if(i==8){name.Buffer=(PWSTR)guard;name.Length=name.Maximum=2;}if(i==9)input=(ATTR*)guard;if(i==10)output=NULL;if(i==11){memcpy(unaligned+1,&a,sizeof(a));input=(ATTR*)(unaligned+1);}if(i==12)output=(PHANDLE)readonly;if(i==13)a.Root=(HANDLE)(ULONG_PTR)0x12345678;
        h=(HANDLE)(ULONG_PTR)0x12345678;before=measure(FALSE,output,access[k],input,0);
        h=(HANDLE)(ULONG_PTR)0x12345678;after=measure(TRUE,output,access[k],input,0);
        s=memcmp(&before,&after,sizeof(before));if(s)++failures;
        fprintf(out,"Case=%u Attr=%u Access=%08lx Status=%08lx Match=%d Granted=%08lx Error=%08lx TLS=%08lx\n",i,j,access[k],after.Status,s==0,after.Access,after.Error,after.TLS);
    }
    if(!nativePresent)for(i=0;i<8;++i){h=(HANDLE)(ULONG_PTR)0x12345678;after=measure(TRUE,&h,KEY_READ,NULL,options[i]);if(after.Status!=(LONG)0xc00000bb || after.Changed)++failures;fprintf(out,"Unsupported=%08lx Status=%08lx OutputUntouched=%d\n",options[i],after.Status,!after.Changed);}
    name.Buffer=L"\\Registry\\Machine\\SYSTEM";name.Length=(USHORT)(wcslen(name.Buffer)*2);name.Maximum=name.Length;ZeroMemory(&a,sizeof(a));a.Length=sizeof(a);a.Name=&name;a.Attributes=0x40;
    if(nativePresent)for(i=0;i<8;++i){
        OPENEX adapter=ex;
        ex=(OPENEX)GetProcAddress(native,"NtOpenKeyEx");h=(HANDLE)(ULONG_PTR)0x12345678;before=measure(TRUE,&h,KEY_READ,&a,options[i]);
        ex=adapter;h=(HANDLE)(ULONG_PTR)0x12345678;after=measure(TRUE,&h,KEY_READ,&a,options[i]);
        s=memcmp(&before,&after,sizeof(before));if(s)++failures;
        fprintf(out,"Delegated=%08lx Status=%08lx Match=%d\n",options[i],after.Status,s==0);
    }
    GetProcessHandleCount(GetCurrentProcess(),&startCount);for(i=0;i<1000;++i){h=NULL;s=ex(&h,KEY_READ,&a,0);if(s<0 || !CloseHandle(h))++failures;}GetProcessHandleCount(GetCurrentProcess(),&endCount);
    fprintf(out,"Repeated=1000 HandleDelta=%ld\n",(LONG)endCount-(LONG)startCount);if(endCount!=startCount)++failures;
    CloseHandle(root);VirtualFree(guard,0,MEM_RELEASE);VirtualFree(readonly,0,MEM_RELEASE);
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
