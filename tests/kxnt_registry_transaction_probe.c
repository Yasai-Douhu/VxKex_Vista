// Owned HKCU fixtures only; never commit a newly created transactional key.
#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <ktmw32.h>
#include <stdio.h>
typedef struct {USHORT Length,MaximumLength;PWSTR Buffer;} USTRING;
typedef struct {ULONG Length;HANDLE Root;USTRING *Name;ULONG Attributes;PVOID Security,Qos;} ATTRIBUTES;
typedef LONG (WINAPI *OPEN_EX)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG);
typedef LONG (WINAPI *QUERY_OBJECT)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *CREATE_KEY_TX)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG,USTRING*,ULONG,HANDLE,PULONG);
typedef LONG (WINAPI *QUERY_VALUE)(HANDLE,USTRING*,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *SET_VALUE)(HANDLE,USTRING*,ULONG,ULONG,PVOID,ULONG);
typedef struct {DWORD Count;LUID_AND_ATTRIBUTES Privileges[2];} TWO_PRIVILEGES;
static FILE *out;static unsigned failures;static QUERY_OBJECT query;static CREATE_KEY_TX createTx;static QUERY_VALUE queryValue;static SET_VALUE setValue;
static ULONG access(HANDLE handle) {
    BYTE data[56];LONG s=query(handle,0,data,sizeof(data),NULL);
    if(s<0){fprintf(out,"ObjectBasicStatus=%08lx\n",s);++failures;return 0;}return *(ULONG*)(data+4);
}
static void transaction(HKEY root,HKEY target,unsigned mode,BOOL missing) {
    HANDLE tx=CreateTransaction(NULL,NULL,0,0,0,5000,L"Owned VxKex registry opening test");HKEY key=NULL,check=NULL;
    DWORD disposition=0,value=0,beforeValue=0,writeValue=0x69133742;LONG create,before=0,after=0,write=0;LSTATUS absent;BOOL finished;ULONG mask=0;FILETIME beforeWrite,afterWrite;
    ATTRIBUTES attrs;USTRING name,marker;BYTE data[128];ULONG needed;
    if(tx==INVALID_HANDLE_VALUE){fprintf(out,"TransactionCreateError=%lu\n",GetLastError());++failures;return;}
    ZeroMemory(&beforeWrite,sizeof(beforeWrite));if(RegQueryInfoKeyW(target,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&beforeWrite))++failures;
    name.Buffer=missing?L"Missing":L"Target";name.Length=(USHORT)(wcslen(name.Buffer)*2);name.MaximumLength=name.Length;
    ZeroMemory(&attrs,sizeof(attrs));attrs.Length=sizeof(attrs);attrs.Root=root;attrs.Name=&name;attrs.Attributes=0x40;
    marker.Buffer=L"Marker";marker.Length=12;marker.MaximumLength=12;
    create=createTx((PHANDLE)&key,KEY_READ,&attrs,0,NULL,REG_OPTION_BACKUP_RESTORE,tx,&disposition);
    if(!create){mask=access(key);before=queryValue(key,&marker,2,data,sizeof(data),&needed);if(before>=0 && *(ULONG*)(data+8)==4)beforeValue=*(ULONG*)(data+12);}
    finished=!create && disposition==REG_OPENED_EXISTING_KEY?CommitTransaction(tx):RollbackTransaction(tx);
    if(!finished)++failures;
    if(!create){after=queryValue(key,&marker,2,data,sizeof(data),&needed);if(after>=0 && *(ULONG*)(data+8)==4)value=*(ULONG*)(data+12);write=setValue(key,&marker,0,REG_DWORD,&writeValue,sizeof(writeValue));}
    absent=RegOpenKeyExW(root,L"Missing",0,KEY_READ,&check);if(check)RegCloseKey(check);
    ZeroMemory(&afterWrite,sizeof(afterWrite));if(RegQueryInfoKeyW(target,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&afterWrite))++failures;
    fprintf(out,"Mode=%u Missing=%d Create=%08lx Disposition=%lu Access=%08lx Finish=%d QueryIssued=%d BeforeQuery=%08lx AfterQuery=%08lx Marker=%08lx MissingKeyOpen=%ld TargetTimestampSame=%d\n",mode,missing,create,disposition,mask,finished,!create,before,after,value,absent,CompareFileTime(&beforeWrite,&afterWrite)==0);
    if(!create)fprintf(out,"PostFinish Mode=%u Missing=%d BeforeMarker=%08lx ReadStatus=%08lx WriteStatus=%08lx\n",mode,missing,beforeValue,after,write);
    if(absent!=ERROR_FILE_NOT_FOUND || CompareFileTime(&beforeWrite,&afterWrite)!=0)++failures;
    if(!create && !missing && disposition!=REG_OPENED_EXISTING_KEY)++failures;
    if(key)RegCloseKey(key);CloseHandle(tx);
}
int main(int argc,char **argv) {
    HMODULE nt=GetModuleHandleW(L"ntdll.dll");OPEN_EX native;HANDLE primary=NULL,token=NULL,oldThread=NULL;
    BOOL hadThread=FALSE,impersonated=FALSE;HKEY root=NULL,target=NULL;DWORD disposition,value=0x69133742;WCHAR path[160];TWO_PRIVILEGES privileges;unsigned mode;LSTATUS err;LONG s;
    USTRING name;ATTRIBUTES attrs;HANDLE key;
    if(argc!=2)return 2;out=fopen(argv[1],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    query=(QUERY_OBJECT)GetProcAddress(nt,"NtQueryObject");native=(OPEN_EX)GetProcAddress(nt,"NtOpenKeyEx");
    createTx=(CREATE_KEY_TX)GetProcAddress(nt,"NtCreateKeyTransacted");queryValue=(QUERY_VALUE)GetProcAddress(nt,"NtQueryValueKey");setValue=(SET_VALUE)GetProcAddress(nt,"NtSetValueKey");if(!query || !createTx || !queryValue || !setValue)return 4;
    swprintf_s(path,160,L"Software\\VxKexProbe\\TxnOpen-%lu-%lu",GetCurrentProcessId(),GetTickCount());
    err=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&root,&disposition);
    if(err || disposition!=REG_CREATED_NEW_KEY){fprintf(out,"FixtureCreate=%ld Disposition=%lu\n",err,disposition);if(root)RegCloseKey(root);fclose(out);return 5;}
    if(RegCreateKeyExW(root,L"Target",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&target,&disposition) || RegSetValueExW(target,L"Marker",0,REG_DWORD,(BYTE*)&value,sizeof(value))){++failures;goto Cleanup;}
    if(OpenThreadToken(GetCurrentThread(),TOKEN_IMPERSONATE,TRUE,&oldThread))hadThread=TRUE;else if(GetLastError()!=ERROR_NO_TOKEN){++failures;goto Cleanup;}
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY,&primary) || !DuplicateTokenEx(primary,TOKEN_ALL_ACCESS,NULL,SecurityImpersonation,TokenImpersonation,&token)){++failures;goto Cleanup;}
    if(!LookupPrivilegeValueW(NULL,L"SeBackupPrivilege",&privileges.Privileges[0].Luid) || !LookupPrivilegeValueW(NULL,L"SeRestorePrivilege",&privileges.Privileges[1].Luid)){++failures;goto Cleanup;}
    privileges.Count=2;
    if(!SetThreadToken(NULL,token)){++failures;goto Cleanup;}impersonated=TRUE;
    fprintf(out,"Bits=%u NativeExPresent=%d Fixture=%ls\n",(unsigned)(sizeof(void*)*8),native!=NULL,path);
    for(mode=0;mode<4;++mode) {
        privileges.Privileges[0].Attributes=(mode&1)?SE_PRIVILEGE_ENABLED:0;privileges.Privileges[1].Attributes=(mode&2)?SE_PRIVILEGE_ENABLED:0;
        SetLastError(0);if(!AdjustTokenPrivileges(token,FALSE,(TOKEN_PRIVILEGES*)&privileges,0,NULL,NULL) || (GetLastError()==ERROR_NOT_ALL_ASSIGNED && mode!=0)){fprintf(out,"Mode=%u PrivilegesUnavailable=1\n",mode);continue;}
        if(native){name.Buffer=L"Target";name.Length=12;name.MaximumLength=12;ZeroMemory(&attrs,sizeof(attrs));attrs.Length=sizeof(attrs);attrs.Root=root;attrs.Name=&name;attrs.Attributes=0x40;key=NULL;s=native(&key,KEY_READ,&attrs,4);fprintf(out,"Native Mode=%u Status=%08lx Access=%08lx\n",mode,s,s>=0?access(key):0);if(key)CloseHandle(key);}
        transaction(root,target,mode,FALSE);transaction(root,target,mode,TRUE);
    }
Cleanup:
    if(impersonated && !SetThreadToken(NULL,hadThread?oldThread:NULL))++failures;
    if(token)CloseHandle(token);if(primary)CloseHandle(primary);if(oldThread)CloseHandle(oldThread);
    if(target)RegCloseKey(target);
    if(root){RegDeleteKeyW(root,L"Missing");if(RegDeleteKeyW(root,L"Target"))++failures;RegCloseKey(root);}
    if(RegDeleteKeyW(HKEY_CURRENT_USER,path))++failures;
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
