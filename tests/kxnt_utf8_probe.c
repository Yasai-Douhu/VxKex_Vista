#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef LONG (WINAPI *CONVERT)(PVOID,ULONG,PULONG,const void*,ULONG);
typedef LONG (WINAPI *GET_STATUS)(void);
typedef VOID (WINAPI *SET_STATUS)(LONG);
typedef struct {const char *Name;const void *Data;ULONG Bytes;} UTF_INPUT;
static FILE *out;static GET_STATUS getstatus;static SET_STATUS seedstatus;static unsigned failures;
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
int main(int argc,char **argv) {
    HMODULE module,native=GetModuleHandleW(L"ntdll.dll");CONVERT from,to;unsigned i;ULONG cap;char path[MAX_PATH];
    if(argc!=3)return 2;out=fopen(argv[2],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    getstatus=(GET_STATUS)GetProcAddress(native,"RtlGetLastNtStatus");seedstatus=(SET_STATUS)GetProcAddress(native,"RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    module=!strcmp(argv[1],"native")?native:LoadLibraryA(argv[1]);if(!module || !getstatus || !seedstatus)return 4;
    GetModuleFileNameA(module,path,sizeof(path));fprintf(out,"Bits=%u Provider=%s\n",(unsigned)(sizeof(void*)*8),path);
    from=(CONVERT)GetProcAddress(module,"RtlUTF8ToUnicodeN");to=(CONVERT)GetProcAddress(module,"RtlUnicodeToUTF8N");
    if(!from || !to){fprintf(out,"ExportsPresent=0\n");fclose(out);return 5;}
    fprintf(out,"ExportsPresent=1\n");
    for(i=0;i<sizeof(utf8)/sizeof(utf8[0]);++i){for(cap=0;cap<=24;++cap)call("decode",from,&utf8[i],cap,FALSE,TRUE);call("decode",from,&utf8[i],0,TRUE,TRUE);call("decode",from,&utf8[i],1,TRUE,TRUE);call("decode",from,&utf8[i],32,FALSE,FALSE);call("decode",from,&utf8[i],0,TRUE,FALSE);}
    for(i=0;i<sizeof(utf16)/sizeof(utf16[0]);++i){for(cap=0;cap<=24;++cap)call("encode",to,&utf16[i],cap,FALSE,TRUE);call("encode",to,&utf16[i],0,TRUE,TRUE);call("encode",to,&utf16[i],1,TRUE,TRUE);call("encode",to,&utf16[i],32,FALSE,FALSE);call("encode",to,&utf16[i],0,TRUE,FALSE);}
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
