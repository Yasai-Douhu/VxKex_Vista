#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <stdio.h>
#define BASE L"C:\\VxKexProbe\\KxNtIfeo"
static FILE *out;static unsigned failures;
static void check(BOOL ok,const char *name){fprintf(out,"%s %s Error=%lu\n",ok?"PASS":"FAIL",name,GetLastError());fflush(out);if(!ok)++failures;}
static BOOL absent(PCWSTR path){HKEY key;LONG s=RegOpenKeyExW(HKEY_LOCAL_MACHINE,path,0,KEY_READ|KEY_WOW64_64KEY,&key);if(!s)RegCloseKey(key);return s==ERROR_FILE_NOT_FOUND;}
static BOOL same(PCWSTR a,PCWSTR b){HANDLE x,y;BYTE bx[4096],by[4096];DWORD nx,ny;BOOL ok=FALSE;x=CreateFileW(a,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);y=CreateFileW(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);if(x==INVALID_HANDLE_VALUE || y==INVALID_HANDLE_VALUE)goto done;for(;;){if(!ReadFile(x,bx,sizeof(bx),&nx,NULL) || !ReadFile(y,by,sizeof(by),&ny,NULL) || nx!=ny || memcmp(bx,by,nx))break;if(!nx){ok=TRUE;break;}}done:if(x!=INVALID_HANDLE_VALUE)CloseHandle(x);if(y!=INVALID_HANDLE_VALUE)CloseHandle(y);return ok;}
static DWORD run(PCWSTR file,PCWSTR args){WCHAR command[4096];STARTUPINFOW si;PROCESS_INFORMATION pi;DWORD wait,code;_snwprintf(command,4096,L"\"%s\" %s",file,args);command[4095]=0;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);ZeroMemory(&pi,sizeof(pi));if(!CreateProcessW(file,command,NULL,NULL,FALSE,0,NULL,L"C:\\Windows",&si,&pi))return GetLastError();wait=WaitForSingleObject(pi.hProcess,60000);if(wait!=WAIT_OBJECT_0){check(FALSE,"owned child did not exit naturally");TerminateProcess(pi.hProcess,0xdead);WaitForSingleObject(pi.hProcess,5000);}GetExitCodeProcess(pi.hProcess,&code);fprintf(out,"Child=%ls PID=%lu Wait=%08lx Exit=%08lx\n",file,pi.dwProcessId,wait,code);fflush(out);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code;}
int main(void){
 const char marker[]="VxKex setup lifecycle disposable VM 20261001";char data[sizeof(marker)];HANDLE f,token=NULL;DWORD bytes,count;PTOKEN_USER user=NULL;PWSTR sid=NULL;
 WCHAR args[2048],image[MAX_PATH],profile[MAX_PATH],source[MAX_PATH],dest[MAX_PATH];BOOL attempted=FALSE,profileAttempted[2]={FALSE,FALSE};unsigned arch,index;
 const WCHAR *arches[]={L"x64",L"x86"};const WCHAR *names[]={L"KexDll.dll",L"KxNt.dll"};
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 out=fopen("C:\\VxKexProbe\\KxNtIfeo\\deployment.txt","w");if(!out)return 2;
 f=CreateFileW(L"C:\\VxKexProbe\\NextParity\\DisposableVM.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 check(f!=INVALID_HANDLE_VALUE && ReadFile(f,data,sizeof(data),&count,NULL) && count==sizeof(marker)-1 && !memcmp(data,marker,count),"exact disposable VM marker");if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 check(IsUserAnAdmin(),"elevated operator");
 check(GetFileAttributesW(L"C:\\VxKex")==INVALID_FILE_ATTRIBUTES && absent(L"Software\\VXsoft\\VxKex"),"fresh deployment required");
 for(arch=0;arch<2;++arch){_snwprintf(profile,MAX_PATH,L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\KxNtIfeoOpen-%s.exe",arches[arch]);check(absent(profile),"never adopt existing IFEO fixture");}
 if(failures)goto done;
 check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token),"capture actual operator token");if(failures)goto done;
 GetTokenInformation(token,TokenUser,NULL,0,&bytes);user=(PTOKEN_USER)HeapAlloc(GetProcessHeap(),0,bytes);
 check(user && GetTokenInformation(token,TokenUser,user,bytes,&bytes) && ConvertSidToStringSidW(user->User.Sid,&sid),"loaded operator SID");if(failures)goto done;
 /* Negative control uses the same image with no IFEO and no helper DLL. */
 for(arch=0;arch<2;++arch){_snwprintf(image,MAX_PATH,BASE L"\\KxNtIfeoOpen-%s.exe",arches[arch]);_snwprintf(args,2048,L"unused " BASE L"\\native-%s.txt",arches[arch]);check(run(image,args)==0xc0000139,"unregistered static import fails before main");}
 if(failures)goto done;
 _snwprintf(args,2048,L"--install \"" BASE L"\\Package\" --user-sid %s",sid);attempted=TRUE;
 check(!run(BASE L"\\Package\\VistaSetup.exe",args),"install full candidate using real setup");if(failures)goto cleanup;
 for(arch=0;arch<2;++arch)for(index=0;index<2;++index){_snwprintf(source,MAX_PATH,BASE L"\\Package\\%s%s",arch?L"Kex32\\":L"",names[index]);_snwprintf(dest,MAX_PATH,L"C:\\Windows\\%s\\%s",arch?L"SysWOW64":L"System32",names[index]);check(same(source,dest),"installed KexDll/KxNt byte-identical to candidate");}
 for(arch=0;arch<2;++arch)for(index=0;index<2;++index){_snwprintf(source,MAX_PATH,BASE L"\\Package\\%s%s",arch?L"Kex32\\":L"",names[index]);_snwprintf(dest,MAX_PATH,L"C:\\VxKex\\%s%s",arch?L"Kex32\\":L"",names[index]);check(same(source,dest),"installed package-root DLL byte-identical to candidate");}
 if(failures)goto cleanup;
 for(arch=0;arch<2;++arch){
  _snwprintf(image,MAX_PATH,BASE L"\\KxNtIfeoOpen-%s.exe",arches[arch]);_snwprintf(args,2048,L"/ADD \"%s\"",image);profileAttempted[arch]=TRUE;
  check(!run(BASE L"\\Package\\KexCfg.exe",args),"enable exact diagnostic image through real KexCfg");
  _snwprintf(args,2048,L"unused " BASE L"\\applied-%s.txt",arches[arch]);check(!run(image,args),"registered static import and full ordinary-open comparisons");
 }
cleanup:
 for(arch=0;arch<2;++arch)if(profileAttempted[arch]){_snwprintf(args,2048,L"/DELETE \"" BASE L"\\KxNtIfeoOpen-%s.exe\"",arches[arch]);check(!run(BASE L"\\Package\\KexCfg.exe",args),"remove owned IFEO fixture");}
 if(attempted){_snwprintf(args,2048,L"--uninstall-remove --user-sid %s",sid);check(!run(BASE L"\\Package\\VistaSetup.exe",args),"remove diagnostic installation");check(GetFileAttributesW(L"C:\\VxKex")==INVALID_FILE_ATTRIBUTES && absent(L"Software\\VXsoft\\VxKex"),"fresh deployment state restored");}
 for(arch=0;arch<2;++arch){_snwprintf(profile,MAX_PATH,L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\KxNtIfeoOpen-%s.exe",arches[arch]);check(absent(profile),"owned IFEO key absent after cleanup");}
done:
 if(token)CloseHandle(token);if(sid)LocalFree(sid);if(user)HeapFree(GetProcessHeap(),0,user);
 fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
