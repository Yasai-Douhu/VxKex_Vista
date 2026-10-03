// Owned HKCU fixtures only; never commit a newly created transactional key.
#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <ktmw32.h>
#include <aclapi.h>
#include <stdio.h>
typedef struct {USHORT Length,MaximumLength;PWSTR Buffer;} USTRING;
typedef struct {ULONG Length;HANDLE Root;USTRING *Name;ULONG Attributes;PVOID Security,Qos;} ATTRIBUTES;
typedef LONG (WINAPI *OPEN_EX)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG);
typedef LONG (WINAPI *OPEN_KEY)(PHANDLE,ACCESS_MASK,ATTRIBUTES*);
typedef LONG (WINAPI *CREATE_KEY)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG,USTRING*,ULONG,PULONG);
typedef LONG (WINAPI *QUERY_OBJECT)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *CREATE_KEY_TX)(PHANDLE,ACCESS_MASK,ATTRIBUTES*,ULONG,USTRING*,ULONG,HANDLE,PULONG);
typedef LONG (WINAPI *QUERY_VALUE)(HANDLE,USTRING*,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *SET_VALUE)(HANDLE,USTRING*,ULONG,ULONG,PVOID,ULONG);
typedef LONG (WINAPI *SET_SECURITY)(HANDLE,ULONG,PSECURITY_DESCRIPTOR);
typedef LONG (WINAPI *QUERY_SECURITY)(HANDLE,ULONG,PSECURITY_DESCRIPTOR,ULONG,PULONG);
typedef LONG (WINAPI *DELETE_KEY)(HANDLE);
typedef struct {DWORD Count;LUID_AND_ATTRIBUTES Privileges[2];} TWO_PRIVILEGES;
static FILE *out;static unsigned failures;static QUERY_OBJECT query;static CREATE_KEY_TX createTx;static QUERY_VALUE queryValue;static SET_VALUE setValue;static OPEN_KEY openKey;static CREATE_KEY createKey;
#define FAIL() do { fprintf(out,"FailureLine=%u\n",(unsigned)__LINE__); ++failures; } while(0)
static ULONG access(HANDLE handle) {
    BYTE data[56];LONG s=query(handle,0,data,sizeof(data),NULL);
    if(s<0){fprintf(out,"ObjectBasicStatus=%08lx\n",s);FAIL();return 0;}return *(ULONG*)(data+4);
}
static void pinned(HKEY root,unsigned mode,BOOL missing,ACCESS_MASK pinAccess,unsigned restricted) {
    USTRING name,empty,marker;ATTRIBUTES attrs;HANDLE pin=NULL,key=NULL;
    LONG opened,created=(LONG)0xdeadbeef,read=(LONG)0xdeadbeef,write=(LONG)0xdeadbeef;
    ULONG disposition=0,mask=0,needed,value=0x69133742;BYTE data[128];
    name.Buffer=missing?L"Missing":L"Target";name.Length=(USHORT)(wcslen(name.Buffer)*2);name.MaximumLength=name.Length;
    ZeroMemory(&attrs,sizeof(attrs));attrs.Length=sizeof(attrs);attrs.Root=root;attrs.Name=&name;attrs.Attributes=0x40;
    opened=openKey(&pin,pinAccess,&attrs);
    if(opened>=0) {
        empty.Buffer=L"";empty.Length=empty.MaximumLength=0;attrs.Root=pin;attrs.Name=&empty;
        created=createKey(&key,KEY_READ,&attrs,0,NULL,REG_OPTION_BACKUP_RESTORE,&disposition);
        if(created>=0) {
            mask=access(key);marker.Buffer=L"Marker";marker.Length=marker.MaximumLength=12;
            read=queryValue(key,&marker,2,data,sizeof(data),&needed);
            if(read>=0 && (*(ULONG*)(data+8)!=4 || *(ULONG*)(data+12)!=value))FAIL();
            write=setValue(key,&marker,0,REG_DWORD,&value,sizeof(value));
            if(disposition!=REG_OPENED_EXISTING_KEY)FAIL();
            if(!CloseHandle(key)){fprintf(out,"CloseKeyError=%lu\n",GetLastError());FAIL();}
        }
        if(!CloseHandle(pin)){fprintf(out,"ClosePinError=%lu\n",GetLastError());FAIL();}
    }
    if(missing && opened!=(LONG)0xc0000034){fprintf(out,"MissingStatusUnexpected=%08lx\n",opened);FAIL();}
    fprintf(out,"Pinned Restricted=%d PinAccess=%08lx Mode=%u Missing=%d Open=%08lx Create=%08lx Disposition=%lu Access=%08lx Read=%08lx Write=%08lx\n",restricted,pinAccess,mode,missing,opened,created,disposition,mask,read,write);
}
static void transaction(HKEY root,HKEY target,unsigned mode,BOOL missing) {
    HANDLE tx=CreateTransaction(NULL,NULL,0,0,0,5000,L"Owned VxKex registry opening test");HKEY key=NULL,check=NULL;
    DWORD disposition=0,value=0,beforeValue=0,writeValue=0x69133742;LONG create,before=0,after=0,write=0;LSTATUS absent;BOOL finished;ULONG mask=0;FILETIME beforeWrite,afterWrite;
    ATTRIBUTES attrs;USTRING name,marker;BYTE data[128];ULONG needed;
    HANDLE reopened=NULL;USTRING empty;ATTRIBUTES reopenAttrs;ULONG reopenedDisposition=0,reopenedAccess=0;
    LONG reopenStatus=(LONG)0xdeadbeef,reopenBefore=(LONG)0xdeadbeef,reopenAfter=(LONG)0xdeadbeef;
    LONG postReopenStatus=(LONG)0xdeadbeef,postRead=(LONG)0xdeadbeef;ULONG postDisposition=0;
    if(tx==INVALID_HANDLE_VALUE){fprintf(out,"TransactionCreateError=%lu\n",GetLastError());FAIL();return;}
    ZeroMemory(&beforeWrite,sizeof(beforeWrite));if(RegQueryInfoKeyW(target,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&beforeWrite))FAIL();
    name.Buffer=missing?L"Missing":L"Target";name.Length=(USHORT)(wcslen(name.Buffer)*2);name.MaximumLength=name.Length;
    ZeroMemory(&attrs,sizeof(attrs));attrs.Length=sizeof(attrs);attrs.Root=root;attrs.Name=&name;attrs.Attributes=0x40;
    marker.Buffer=L"Marker";marker.Length=12;marker.MaximumLength=12;
    create=createTx((PHANDLE)&key,KEY_READ,&attrs,0,NULL,REG_OPTION_BACKUP_RESTORE,tx,&disposition);
    if(!create){mask=access(key);before=queryValue(key,&marker,2,data,sizeof(data),&needed);if(before>=0 && *(ULONG*)(data+8)==4)beforeValue=*(ULONG*)(data+12);}
    if(!create && disposition==REG_OPENED_EXISTING_KEY){
        empty.Buffer=L"";empty.Length=empty.MaximumLength=0;reopenAttrs=attrs;reopenAttrs.Root=key;reopenAttrs.Name=&empty;
        reopenStatus=createKey(&reopened,KEY_READ,&reopenAttrs,0,NULL,REG_OPTION_BACKUP_RESTORE,&reopenedDisposition);
        if(reopenStatus>=0){
            reopenedAccess=access(reopened);reopenBefore=queryValue(reopened,&marker,2,data,sizeof(data),&needed);
            if(reopenedDisposition!=REG_OPENED_EXISTING_KEY)FAIL();
            if(reopenBefore>=0 && (*(ULONG*)(data+8)!=4 || *(ULONG*)(data+12)!=0x69133742))FAIL();
        }
    }
    finished=!create && disposition==REG_OPENED_EXISTING_KEY?CommitTransaction(tx):RollbackTransaction(tx);
    if(!finished)FAIL();
    if(reopened){
        reopenAfter=queryValue(reopened,&marker,2,data,sizeof(data),&needed);
        if(reopenAfter>=0 && (*(ULONG*)(data+8)!=4 || *(ULONG*)(data+12)!=0x69133742))FAIL();
        if(!CloseHandle(reopened))FAIL();
    }
    if(!create && disposition==REG_OPENED_EXISTING_KEY){
        reopened=NULL;postReopenStatus=createKey(&reopened,KEY_READ,&reopenAttrs,0,NULL,REG_OPTION_BACKUP_RESTORE,&postDisposition);
        if(postReopenStatus>=0){
            postRead=queryValue(reopened,&marker,2,data,sizeof(data),&needed);
            if(postDisposition!=REG_OPENED_EXISTING_KEY)FAIL();
            if(postRead>=0 && (*(ULONG*)(data+8)!=4 || *(ULONG*)(data+12)!=0x69133742))FAIL();
            if(!CloseHandle(reopened))FAIL();
        }
    }
    fprintf(out,"ReopenTransaction Mode=%u Missing=%d Create=%08lx Disposition=%lu Access=%08lx BeforeRead=%08lx AfterRead=%08lx\n",mode,missing,reopenStatus,reopenedDisposition,reopenedAccess,reopenBefore,reopenAfter);
    fprintf(out,"PostCommitReopen Mode=%u Missing=%d Create=%08lx Disposition=%lu Read=%08lx\n",mode,missing,postReopenStatus,postDisposition,postRead);
    if(!create){after=queryValue(key,&marker,2,data,sizeof(data),&needed);if(after>=0 && *(ULONG*)(data+8)==4)value=*(ULONG*)(data+12);write=setValue(key,&marker,0,REG_DWORD,&writeValue,sizeof(writeValue));}
    absent=RegOpenKeyExW(root,L"Missing",0,KEY_READ,&check);if(check)RegCloseKey(check);
    ZeroMemory(&afterWrite,sizeof(afterWrite));if(RegQueryInfoKeyW(target,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&afterWrite))FAIL();
    fprintf(out,"Mode=%u Missing=%d Create=%08lx Disposition=%lu Access=%08lx Finish=%d QueryIssued=%d BeforeQuery=%08lx AfterQuery=%08lx Marker=%08lx MissingKeyOpen=%ld TargetTimestampSame=%d\n",mode,missing,create,disposition,mask,finished,!create,before,after,value,absent,CompareFileTime(&beforeWrite,&afterWrite)==0);
    if(!create)fprintf(out,"PostFinish Mode=%u Missing=%d BeforeMarker=%08lx ReadStatus=%08lx WriteStatus=%08lx\n",mode,missing,beforeValue,after,write);
    if(absent!=ERROR_FILE_NOT_FOUND || CompareFileTime(&beforeWrite,&afterWrite)!=0)FAIL();
    if(!create && !missing && disposition!=REG_OPENED_EXISTING_KEY)FAIL();
    if(key)RegCloseKey(key);CloseHandle(tx);
}
int main(int argc,char **argv) {
    HMODULE nt=GetModuleHandleW(L"ntdll.dll");OPEN_EX native;HANDLE primary=NULL,token=NULL,oldThread=NULL;
    BOOL hadThread=FALSE,impersonated=FALSE;HKEY root=NULL,target=NULL;DWORD disposition,value=0x69133742;WCHAR path[160];TWO_PRIVILEGES privileges;unsigned mode;LSTATUS err;LONG s;
    USTRING name;ATTRIBUTES attrs;HANDLE key;
    ACCESS_MASK pinAccesses[]={0,KEY_QUERY_VALUE,READ_CONTROL,MAXIMUM_ALLOWED};
    unsigned restricted,index;ACL denyAll;PACL oldAcl=NULL;PSECURITY_DESCRIPTOR oldSecurity=NULL;BOOL daclChanged=FALSE;
    BYTE ownerDaclStorage[sizeof(ACL)+sizeof(ACCESS_DENIED_ACE)+SECURITY_MAX_SID_SIZE];
    BYTE ownerSidStorage[SECURITY_MAX_SID_SIZE];PSID ownerSid=ownerSidStorage;
    SID_IDENTIFIER_AUTHORITY creatorAuthority=SECURITY_CREATOR_SID_AUTHORITY;
    PACL ownerDacl=(PACL)ownerDaclStorage;
    SET_SECURITY setSecurity=(SET_SECURITY)GetProcAddress(nt,"NtSetSecurityObject");
    QUERY_SECURITY querySecurity=(QUERY_SECURITY)GetProcAddress(nt,"NtQuerySecurityObject");
    DELETE_KEY deleteKey=(DELETE_KEY)GetProcAddress(nt,"NtDeleteKey");
    if(argc!=2)return 2;out=fopen(argv[1],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    query=(QUERY_OBJECT)GetProcAddress(nt,"NtQueryObject");native=(OPEN_EX)GetProcAddress(nt,"NtOpenKeyEx");
    createTx=(CREATE_KEY_TX)GetProcAddress(nt,"NtCreateKeyTransacted");queryValue=(QUERY_VALUE)GetProcAddress(nt,"NtQueryValueKey");setValue=(SET_VALUE)GetProcAddress(nt,"NtSetValueKey");
    openKey=(OPEN_KEY)GetProcAddress(nt,"NtOpenKey");createKey=(CREATE_KEY)GetProcAddress(nt,"NtCreateKey");if(!query || !createTx || !queryValue || !setValue || !openKey || !createKey || !setSecurity || !querySecurity || !deleteKey)return 4;
    swprintf_s(path,160,L"Software\\VxKexProbe\\TxnOpen-%lu-%lu",GetCurrentProcessId(),GetTickCount());
    err=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&root,&disposition);
    if(err || disposition!=REG_CREATED_NEW_KEY){fprintf(out,"FixtureCreate=%ld Disposition=%lu\n",err,disposition);if(root)RegCloseKey(root);fclose(out);return 5;}
    if(RegCreateKeyExW(root,L"Target",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&target,&disposition) || RegSetValueExW(target,L"Marker",0,REG_DWORD,(BYTE*)&value,sizeof(value))){FAIL();goto Cleanup;}
    if(OpenThreadToken(GetCurrentThread(),TOKEN_IMPERSONATE,TRUE,&oldThread))hadThread=TRUE;else if(GetLastError()!=ERROR_NO_TOKEN){FAIL();goto Cleanup;}
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY,&primary) || !DuplicateTokenEx(primary,TOKEN_ALL_ACCESS,NULL,SecurityImpersonation,TokenImpersonation,&token)){FAIL();goto Cleanup;}
    if(!LookupPrivilegeValueW(NULL,L"SeBackupPrivilege",&privileges.Privileges[0].Luid) || !LookupPrivilegeValueW(NULL,L"SeRestorePrivilege",&privileges.Privileges[1].Luid)){FAIL();goto Cleanup;}
    privileges.Count=2;
    if(!SetThreadToken(NULL,token)){FAIL();goto Cleanup;}impersonated=TRUE;
    fprintf(out,"Bits=%u NativeExPresent=%d Fixture=%ls\n",(unsigned)(sizeof(void*)*8),native!=NULL,path);
    if(GetSecurityInfo(target,SE_REGISTRY_KEY,DACL_SECURITY_INFORMATION,NULL,NULL,&oldAcl,NULL,&oldSecurity)){FAIL();goto Cleanup;}
    if(!InitializeAcl(&denyAll,sizeof(denyAll),ACL_REVISION)){FAIL();goto Cleanup;}
    if(!InitializeSid(ownerSid,&creatorAuthority,1)){FAIL();goto Cleanup;}
    *GetSidSubAuthority(ownerSid,0)=4; // OWNER RIGHTS, S-1-3-4: suppress implicit owner access.
    if(!InitializeAcl(ownerDacl,sizeof(ownerDaclStorage),ACL_REVISION) || !AddAccessDeniedAce(ownerDacl,ACL_REVISION,KEY_ALL_ACCESS,ownerSid)){FAIL();goto Cleanup;}
    for(restricted=0;restricted<3;++restricted) {
    if(restricted){
        // The existing full-access handle remains usable to restore our own fixture.
        PACL actualAcl=NULL;BYTE actualSecurity[1024];ULONG needed;BOOL present,defaulted;
        SECURITY_DESCRIPTOR descriptor;
        if(!InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION) || !SetSecurityDescriptorDacl(&descriptor,TRUE,restricted==1?&denyAll:ownerDacl,FALSE)){FAIL();goto Cleanup;}
        s=setSecurity(target,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,&descriptor);
        if(s<0){fprintf(out,"SetDaclStatus=%08lx\n",s);FAIL();goto Cleanup;}
        daclChanged=TRUE;
        s=querySecurity(target,DACL_SECURITY_INFORMATION,actualSecurity,sizeof(actualSecurity),&needed);
        if(s<0 || !GetSecurityDescriptorDacl(actualSecurity,&present,&actualAcl,&defaulted)){FAIL();goto Cleanup;}
        fprintf(out,"Restricted=%u DaclPresent=%d AceCount=%u\n",restricted,actualAcl!=NULL,actualAcl?actualAcl->AceCount:0xffff);
        if(!actualAcl || actualAcl->AceCount!=restricted-1)FAIL();
        if(failures)goto Cleanup;
    }
    for(mode=0;mode<4;++mode) {
        privileges.Privileges[0].Attributes=(mode&1)?SE_PRIVILEGE_ENABLED:0;privileges.Privileges[1].Attributes=(mode&2)?SE_PRIVILEGE_ENABLED:0;
        SetLastError(0);if(!AdjustTokenPrivileges(token,FALSE,(TOKEN_PRIVILEGES*)&privileges,0,NULL,NULL) || (GetLastError()==ERROR_NOT_ALL_ASSIGNED && mode!=0)){fprintf(out,"Mode=%u PrivilegesUnavailable=1\n",mode);continue;}
        if(native){name.Buffer=L"Target";name.Length=12;name.MaximumLength=12;ZeroMemory(&attrs,sizeof(attrs));attrs.Length=sizeof(attrs);attrs.Root=root;attrs.Name=&name;attrs.Attributes=0x40;key=NULL;s=native(&key,KEY_READ,&attrs,4);fprintf(out,"Native Restricted=%u Mode=%u Status=%08lx Access=%08lx\n",restricted,mode,s,s>=0?access(key):0);if(key)CloseHandle(key);}
        if(!restricted){transaction(root,target,mode,FALSE);transaction(root,target,mode,TRUE);}
        for(index=0;index<sizeof(pinAccesses)/sizeof(pinAccesses[0]);++index){
            pinned(root,mode,FALSE,pinAccesses[index],restricted);
            pinned(root,mode,TRUE,pinAccesses[index],restricted);
        }
    }
    }
Cleanup:
    // This is an exclusively owned, disposable key. Always delete it using the
    // original DELETE handle, even when restoring its DACL is denied.
    if(daclChanged){s=setSecurity(target,DACL_SECURITY_INFORMATION,oldSecurity);fprintf(out,"RestoreDaclStatus=%08lx\n",s);}
    if(oldSecurity)LocalFree(oldSecurity);
    if(impersonated && !SetThreadToken(NULL,hadThread?oldThread:NULL))FAIL();
    if(token)CloseHandle(token);if(primary)CloseHandle(primary);if(oldThread)CloseHandle(oldThread);
    if(target){s=deleteKey(target);fprintf(out,"DeleteOwnedTargetStatus=%08lx\n",s);if(s<0)FAIL();RegCloseKey(target);}
    if(root){RegDeleteKeyW(root,L"Missing");RegCloseKey(root);}
    if(RegDeleteKeyW(HKEY_CURRENT_USER,path))FAIL();
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
