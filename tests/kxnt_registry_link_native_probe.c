#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
typedef struct {USHORT Length,Maximum;PWSTR Buffer;} USTRING;
typedef struct {ULONG Length;HANDLE Root;USTRING *Name;ULONG Attributes;PVOID Security,Qos;} ATTR;
typedef LONG (WINAPI *OPEN)(PHANDLE,ACCESS_MASK,ATTR*);
typedef LONG (WINAPI *OPENEX)(PHANDLE,ACCESS_MASK,ATTR*,ULONG);
typedef LONG (WINAPI *QUERY)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *DELETEKEY)(HANDLE);
static FILE *out;static unsigned failures;
static void check(const char *what,BOOL ok){fprintf(out,"Check=%s Pass=%d\n",what,ok);fflush(out);if(!ok)++failures;}
static BOOL keyname(QUERY query,HANDLE key,WCHAR *text){
    union{ULONG_PTR Align;BYTE Data[2048];} info;ULONG need,bytes;LONG s;
    s=query(key,3,info.Data,sizeof(info.Data),&need);if(s<0)return FALSE;
    bytes=*(ULONG*)info.Data;if(bytes>=1024)return FALSE;
    memcpy(text,info.Data+4,bytes);text[bytes/2]=0;return TRUE;
}
int main(int argc,char **argv){
    HMODULE native=GetModuleHandleW(L"ntdll.dll"),provider=NULL,owner=NULL;OPEN old;OPENEX ex;QUERY query;DELETEKEY remove;BOOL nativePresent;
    char providerPath[MAX_PATH],ownerPath[MAX_PATH];
    HKEY base=NULL,target=NULL,link=NULL,h=NULL;DWORD disposition,value=42;
    WCHAR path[128],baseName[512],targetName[512],actual[512],linkName[512];
    USTRING name;ATTR attr;LONG s;unsigned flags,mode;BOOL owned=FALSE;DWORD start,end;
    if(argc!=2 && argc!=3)return 2;out=fopen(argv[1],"w");if(!out)return 3;
    old=(OPEN)GetProcAddress(native,"NtOpenKey");ex=(OPENEX)GetProcAddress(native,"NtOpenKeyEx");
    query=(QUERY)GetProcAddress(native,"NtQueryKey");remove=(DELETEKEY)GetProcAddress(native,"NtDeleteKey");
    nativePresent=ex!=NULL;
    if(argc==3){
        provider=LoadLibraryA(argv[2]);ex=provider?(OPENEX)GetProcAddress(provider,"NtOpenKeyEx"):NULL;
        if(!ex || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(PCSTR)ex,&owner)){++failures;goto done;}
        GetModuleFileNameA(provider,providerPath,sizeof(providerPath));GetModuleFileNameA(owner,ownerPath,sizeof(ownerPath));
        fprintf(out,"AdapterProvider=%s Implementation=%s\n",providerPath,ownerPath);
    }
    fprintf(out,"ProcessBits=%u NativeEx=%d KexDllLoaded=%d Adapter=%d\n",(unsigned)(sizeof(PVOID)*8),nativePresent,GetModuleHandleW(L"KexDll.dll")!=NULL,argc==3);
    if(!old || !query || !remove || (argc==2 && GetModuleHandleW(L"KexDll.dll"))){++failures;goto done;}
    _snwprintf(path,128,L"Software\\KxNtOwnedLink-%lu-%lu",GetCurrentProcessId(),GetTickCount());
    fprintf(out,"OwnedPath=%ls\n",path);
    s=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,NULL,&base,&disposition);
    check("create-new-owned-base",s==0 && disposition==REG_CREATED_NEW_KEY);
    if(s || disposition!=REG_CREATED_NEW_KEY)goto done;owned=TRUE;
    check("query-base-name",keyname(query,base,baseName));if(failures)goto done;
    s=RegCreateKeyExW(base,L"Target",0,NULL,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,NULL,&target,&disposition);
    check("create-target",s==0);if(s)goto done;
    check("set-marker",RegSetValueExW(target,L"Marker",0,REG_DWORD,(BYTE*)&value,4)==0);
    _snwprintf(targetName,512,L"%s\\Target",baseName);_snwprintf(linkName,512,L"%s\\Link",baseName);
    s=RegCreateKeyExW(base,L"Link",0,NULL,REG_OPTION_VOLATILE|REG_OPTION_CREATE_LINK,KEY_ALL_ACCESS,NULL,&link,&disposition);
    check("create-owned-link",s==0);if(s)goto done;
    check("set-link-target",RegSetValueExW(link,L"SymbolicLinkValue",0,REG_LINK,(BYTE*)targetName,(DWORD)(wcslen(targetName)*2))==0);
    if(failures)goto done;
    name.Buffer=L"Link";name.Length=name.Maximum=8;ZeroMemory(&attr,sizeof(attr));attr.Length=sizeof(attr);attr.Root=base;attr.Name=&name;
    GetProcessHandleCount(GetCurrentProcess(),&start);
    for(mode=0;mode<3;++mode)for(flags=0;flags<(mode==1?4:2);++flags){
        ULONG options=mode==0?0:mode==1?((flags&2)?8:0):(flags?8:0);
        BOOL expectLink=(flags&1)!=0;
        h=NULL;actual[0]=0;attr.Attributes=0x40|(flags?0x100:0);
        if(mode==1)attr.Attributes=0x40|(expectLink?0x100:0);
        if(mode==0)s=old((PHANDLE)&h,KEY_READ,&attr);
        else if(mode==1)s=ex?ex((PHANDLE)&h,KEY_READ,&attr,options):(LONG)0xc00000bb;
        else s=RegOpenKeyExW(base,L"Link",flags?REG_OPTION_OPEN_LINK:0,KEY_READ,&h);
        fprintf(out,"Mode=%s Attributes=%08lx Options=%08lx Status=%08lx\n",mode==0?"NtOpenKey-OBJ":mode==1?"NtOpenKeyEx-option":"RegOpenKeyEx",attr.Attributes,options,s);
        if(s>=0 && h){check("query-opened-name",keyname(query,h,actual));fprintf(out,"Name=%ls Expected=%s\n",actual,!wcscmp(actual,expectLink?linkName:targetName)?"MATCH":"DIFFERENT");check("open-link-semantics",!wcscmp(actual,expectLink?linkName:targetName));RegCloseKey(h);}
        else{fprintf(out,"\n");if(mode!=1 || ex)++failures;}
    }
    GetProcessHandleCount(GetCurrentProcess(),&end);fprintf(out,"HandleDelta=%ld\n",(LONG)end-(LONG)start);check("no-handle-growth",start==end);
done:
    // The original creation handle names the link itself, never its target.
    if(link){check("delete-link-by-owned-handle",remove(link)>=0);RegCloseKey(link);}
    if(target){check("delete-target-by-owned-handle",remove(target)>=0);RegCloseKey(target);}
    if(base)RegCloseKey(base);
    if(owned){check("delete-owned-base",RegDeleteKeyW(HKEY_CURRENT_USER,path)==0);h=NULL;check("owned-base-absent",RegOpenKeyExW(HKEY_CURRENT_USER,path,0,KEY_READ,&h)==ERROR_FILE_NOT_FOUND);if(h)RegCloseKey(h);}
    fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
