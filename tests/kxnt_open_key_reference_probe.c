#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef struct {USHORT Length,MaximumLength;PWSTR Buffer;} USTRING;
typedef struct {ULONG Length;HANDLE Root;USTRING *Name;ULONG Attributes;PVOID Security,Qos;} ATTRIBUTES;
typedef LONG (WINAPI *OPEN_KEY)(PHANDLE,ACCESS_MASK,ATTRIBUTES*);
typedef LONG (WINAPI *OPEN_KEY_EX)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG);
typedef LONG (WINAPI *QUERY_KEY)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *GET_STATUS)(void);
typedef VOID (WINAPI *SET_STATUS)(LONG);
static FILE *out;static QUERY_KEY query;static GET_STATUS getstatus;static SET_STATUS seedstatus;
static unsigned failures;
static void result(const char *method,const char *name,ULONG options,ULONG attributes,LONG status,HANDLE handle) {
    union {ULONG_PTR Alignment;BYTE Bytes[1024];} buffer;ULONG needed;LONG q;
    DWORD error=GetLastError();LONG tls=getstatus();
    fprintf(out,"Method=%s Input=%s Options=%08lx Attributes=%08lx Status=%08lx Error=%08lx TLS=%08lx",method,name,options,attributes,status,error,tls);
    if(status>=0){q=query(handle,3,buffer.Bytes,sizeof(buffer.Bytes),&needed);fprintf(out," Query=%08lx Name=%.*ls",q,q>=0?*(ULONG*)buffer.Bytes/2:0,q>=0?(WCHAR*)(buffer.Bytes+4):L"");CloseHandle(handle);if(q<0)++failures;}
    else fprintf(out," HandleUnchanged=%d",handle==(HANDLE)(ULONG_PTR)0x12345678);
    fputc('\n',out);
    if(error!=0x13579bdf || tls!=(LONG)0xc0000022)++failures;
}
int main(int argc,char **argv) {
    HMODULE native=GetModuleHandleW(L"ntdll.dll");OPEN_KEY old;OPEN_KEY_EX ex;ATTRIBUTES a;USTRING string;
    const WCHAR *paths[]={L"\\Registry\\Machine\\SYSTEM",L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet",L"\\Registry\\Machine\\SYSTEM\\VxKexProbeMissingKey_69133742"};
    const char *names[]={"normal","system-link","missing"};ULONG options[]={0,8,4,12,1,2,16,0xffffffff};
    unsigned i,j;HANDLE key;LONG status;BOOL legacy;
    if(argc!=3)return 2;out=fopen(argv[2],"w");if(!out)return 3;legacy=!strcmp(argv[1],"legacy");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    old=(OPEN_KEY)GetProcAddress(native,"NtOpenKey");ex=(OPEN_KEY_EX)GetProcAddress(native,"NtOpenKeyEx");query=(QUERY_KEY)GetProcAddress(native,"NtQueryKey");
    getstatus=(GET_STATUS)GetProcAddress(native,"RtlGetLastNtStatus");seedstatus=(SET_STATUS)GetProcAddress(native,"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    if(!old || !query || !getstatus || !seedstatus)return 4;
    fprintf(out,"Bits=%u NativeExPresent=%d Legacy=%d\n",(unsigned)(sizeof(void*)*8),ex!=NULL,legacy);
    if(!legacy && !ex){fprintf(out,"Result=NATIVE_EX_ABSENT\n");fclose(out);return 5;}
    for(i=0;i<3;++i)for(j=0;j<(legacy?2:8);++j) {
        string.Buffer=(PWSTR)paths[i];string.Length=(USHORT)(wcslen(paths[i])*2);string.MaximumLength=string.Length;
        ZeroMemory(&a,sizeof(a));a.Length=sizeof(a);a.Name=&string;a.Attributes=0x40|(legacy && j?0x100:0);
        key=(HANDLE)(ULONG_PTR)0x12345678;seedstatus((LONG)0xc0000022);SetLastError(0x13579bdf);
        status=legacy?old(&key,KEY_READ,&a):ex(&key,KEY_READ,&a,options[j]);
        result(legacy?"NtOpenKey":"NtOpenKeyEx",names[i],legacy?0:options[j],a.Attributes,status,key);
    }
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
