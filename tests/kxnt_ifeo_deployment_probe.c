#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#define BASE L"C:\\VxKexProbe\\KxNtIfeo"
static FILE *out;static unsigned failures;
static DWORD lastChild,lastWait;
static BOOL selected(unsigned test,BOOL trace){return !trace || test==17 || test==25;}
static DWORD tracePid(PCWSTR path){FILE *file=_wfopen(path,L"r");char line[1024],*marker;DWORD pid=0;if(!file)return 0;while(fgets(line,sizeof(line),file))if(!strncmp(line,"KXNT_TARGET_PID=",16) && (marker=line)!=NULL){pid=strtoul(marker+16,NULL,16);break;}fclose(file);return pid;}
static const WCHAR *kinds[]={L"processor-feature",L"domain",L"device-family",L"persisted-state",L"sid-package",L"sid-capability",L"membership",L"compare",L"file-information",L"alert",L"performance",L"srw"};
static void fixture(unsigned test,PWSTR image,PWSTR name){const WCHAR *arch=test%2?L"x86":L"x64";if(test<2)_snwprintf(name,MAX_PATH,L"KxNtIfeoOpen-%s.exe",arch);else _snwprintf(name,MAX_PATH,L"KxNtIfeo-%s-%s.exe",kinds[(test-2)/2],arch);_snwprintf(image,MAX_PATH,BASE L"\\%s",name);}
static void check(BOOL ok,const char *name){fprintf(out,"%s %s Error=%lu\n",ok?"PASS":"FAIL",name,GetLastError());fflush(out);if(!ok)++failures;}
static BOOL absent(PCWSTR path){HKEY key;LONG s=RegOpenKeyExW(HKEY_LOCAL_MACHINE,path,0,KEY_READ|KEY_WOW64_64KEY,&key);if(!s)RegCloseKey(key);return s==ERROR_FILE_NOT_FOUND;}
static BOOL same(PCWSTR a,PCWSTR b){HANDLE x,y;BYTE bx[4096],by[4096];DWORD nx,ny;BOOL ok=FALSE;x=CreateFileW(a,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);y=CreateFileW(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);if(x==INVALID_HANDLE_VALUE || y==INVALID_HANDLE_VALUE)goto done;for(;;){if(!ReadFile(x,bx,sizeof(bx),&nx,NULL) || !ReadFile(y,by,sizeof(by),&ny,NULL) || nx!=ny || memcmp(bx,by,nx))break;if(!nx){ok=TRUE;break;}}done:if(x!=INVALID_HANDLE_VALUE)CloseHandle(x);if(y!=INVALID_HANDLE_VALUE)CloseHandle(y);return ok;}
static DWORD run(PCWSTR file,PCWSTR args){WCHAR command[4096];STARTUPINFOW si;PROCESS_INFORMATION pi;DWORD wait,code;lastChild=0;lastWait=WAIT_FAILED;_snwprintf(command,4096,L"\"%s\" %s",file,args);command[4095]=0;ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);ZeroMemory(&pi,sizeof(pi));if(!CreateProcessW(file,command,NULL,NULL,FALSE,0,NULL,L"C:\\Windows",&si,&pi))return GetLastError();lastChild=pi.dwProcessId;wait=WaitForSingleObject(pi.hProcess,60000);lastWait=wait;if(wait!=WAIT_OBJECT_0){check(FALSE,"owned child did not exit naturally");TerminateProcess(pi.hProcess,0xdead);WaitForSingleObject(pi.hProcess,5000);}GetExitCodeProcess(pi.hProcess,&code);fprintf(out,"Child=%ls PID=%lu Wait=%08lx Exit=%08lx\n",file,pi.dwProcessId,wait,code);fflush(out);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code;}
int main(int argc,char **argv){
 const char marker[]="VxKex setup lifecycle disposable VM 20261001";char data[sizeof(marker)];HANDLE f,token=NULL;DWORD bytes,count;PTOKEN_USER user=NULL;PWSTR sid=NULL;
 WCHAR args[2048],image[MAX_PATH],name[MAX_PATH],profile[MAX_PATH],source[MAX_PATH],dest[MAX_PATH];BOOL attempted=FALSE,profileAttempted[26]={FALSE};unsigned arch,index,test,countTests;
 BOOL trace=argc==2 && !strcmp(argv[1],"--event-trace");BOOL suite=trace || (argc==2 && !strcmp(argv[1],"--suite"));
 OSVERSIONINFOEXW version;SYSTEM_INFO system;
 const WCHAR *arches[]={L"x64",L"x86"};const WCHAR *names[]={L"KexDll.dll",L"KxNt.dll"};
 if(argc!=1 && !suite)return 87;countTests=suite?26:2;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 out=fopen("C:\\VxKexProbe\\KxNtIfeo\\deployment.txt","w");if(!out)return 2;
 ZeroMemory(&version,sizeof(version));version.dwOSVersionInfoSize=sizeof(version);check(GetVersionExW((OSVERSIONINFOW*)&version),"read actual clone OS version");GetNativeSystemInfo(&system);
 fprintf(out,"OSVersion=%lu.%lu.%lu ProductType=%u NativeArchitecture=%u DriverKexDllLoaded=%d\n",version.dwMajorVersion,version.dwMinorVersion,version.dwBuildNumber,version.wProductType,system.wProcessorArchitecture,GetModuleHandleW(L"KexDll.dll")!=NULL);
 f=CreateFileW(L"C:\\VxKexProbe\\NextParity\\DisposableVM.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 check(f!=INVALID_HANDLE_VALUE && ReadFile(f,data,sizeof(data),&count,NULL) && count==sizeof(marker)-1 && !memcmp(data,marker,count),"exact disposable VM marker");if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 check(IsUserAnAdmin(),"elevated operator");
 check(GetFileAttributesW(L"C:\\VxKex")==INVALID_FILE_ATTRIBUTES && absent(L"Software\\VXsoft\\VxKex"),"fresh deployment required");
 for(test=0;test<countTests;++test){if(!selected(test,trace))continue;fixture(test,image,name);_snwprintf(profile,MAX_PATH,L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",name);check(absent(profile),"never adopt existing IFEO fixture");}
 if(failures)goto done;
 check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token),"capture actual operator token");if(failures)goto done;
 GetTokenInformation(token,TokenUser,NULL,0,&bytes);user=(PTOKEN_USER)HeapAlloc(GetProcessHeap(),0,bytes);
 check(user && GetTokenInformation(token,TokenUser,user,bytes,&bytes) && ConvertSidToStringSidW(user->User.Sid,&sid),"loaded operator SID");if(failures)goto done;
 /* Negative control uses the same image with no IFEO and no helper DLL. */
 for(test=0;test<countTests;++test){if(!selected(test,trace))continue;fixture(test,image,name);check(run(image,L"")==0xc0000139,"unregistered static import fails before main");}
 if(failures)goto done;
 _snwprintf(args,2048,L"--install \"" BASE L"\\Package\" --user-sid %s",sid);attempted=TRUE;
 check(!run(BASE L"\\Package\\VistaSetup.exe",args),"install full candidate using real setup");if(failures)goto cleanup;
 for(arch=0;arch<2;++arch)for(index=0;index<2;++index){_snwprintf(source,MAX_PATH,BASE L"\\Package\\%s%s",arch?L"Kex32\\":L"",names[index]);_snwprintf(dest,MAX_PATH,L"C:\\Windows\\%s\\%s",arch?L"SysWOW64":L"System32",names[index]);check(same(source,dest),"installed KexDll/KxNt byte-identical to candidate");}
 for(arch=0;arch<2;++arch)for(index=0;index<2;++index){_snwprintf(source,MAX_PATH,BASE L"\\Package\\%s%s",arch?L"Kex32\\":L"",names[index]);_snwprintf(dest,MAX_PATH,L"C:\\VxKex\\%s%s",arch?L"Kex32\\":L"",names[index]);check(same(source,dest),"installed package-root DLL byte-identical to candidate");}
 if(failures)goto cleanup;
 for(test=0;test<countTests;++test){
  if(!selected(test,trace))continue;
  fixture(test,image,name);arch=test%2;_snwprintf(args,2048,L"/ADD \"%s\"",image);profileAttempted[test]=TRUE;
  check(!run(BASE L"\\Package\\KexCfg.exe",args),"enable exact diagnostic image through real KexCfg");
  if(test<2)_snwprintf(args,2048,L"unused " BASE L"\\applied-%s.txt",arches[arch]);
  else _snwprintf(args,2048,L"\"%s\" " BASE L"\\applied-%s-%s.txt %s",arch?L"C:\\VxKex\\Kex32\\KxNt.dll":L"C:\\Windows\\System32\\KxNt.dll",kinds[(test-2)/2],arches[arch],(test-2)/2==4?L"RtlIsPackageSid":(test-2)/2==5?L"RtlIsCapabilitySid":L"");
  if(trace){
   WCHAR debugArgs[4096];DWORD debugExit;_snwprintf(dest,MAX_PATH,BASE L"\\event-%s-x86.log",kinds[(test-2)/2]);
   _snwprintf(debugArgs,4096,L"-G -logo \"%s\" -cf \"" BASE L"\\event-trace.cdb\" \"%s\" %s",dest,image,args);
   debugExit=run(L"C:\\VxKexProbe\\Wow64\\cdb.exe",debugArgs);check(lastWait==WAIT_OBJECT_0 && debugExit<=1,"owned debugger exited naturally; probe exit0/1 recorded separately");
   lastChild=tracePid(dest);check(lastChild!=0,"read actual debuggee PID from CDB");
  }else check(!run(image,args),"registered static imports and detailed comparisons");
  if(test>=2){_snwprintf(source,MAX_PATH,BASE L"\\bindings-%lu.txt",lastChild);_snwprintf(dest,MAX_PATH,BASE L"\\bindings-%s-%s.txt",kinds[(test-2)/2],arches[arch]);check(CopyFileW(source,dest,TRUE),"preserve main-process static bindings");}
 }
cleanup:
 for(test=0;test<countTests;++test)if(profileAttempted[test]){fixture(test,image,name);_snwprintf(args,2048,L"/DELETE \"%s\"",image);check(!run(BASE L"\\Package\\KexCfg.exe",args),"remove owned IFEO fixture");}
 if(attempted){_snwprintf(args,2048,L"--uninstall-remove --user-sid %s",sid);check(!run(BASE L"\\Package\\VistaSetup.exe",args),"remove diagnostic installation");check(GetFileAttributesW(L"C:\\VxKex")==INVALID_FILE_ATTRIBUTES && absent(L"Software\\VXsoft\\VxKex"),"fresh deployment state restored");}
 for(test=0;test<countTests;++test){if(!selected(test,trace))continue;fixture(test,image,name);_snwprintf(profile,MAX_PATH,L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",name);check(absent(profile),"owned IFEO key absent after cleanup");}
done:
 if(token)CloseHandle(token);if(sid)LocalFree(sid);if(user)HeapFree(GetProcessHeap(),0,user);
 fprintf(out,"Failures=%u Result=%s\n",failures,failures?"FAIL":"PASS");fclose(out);return failures?1:0;
}
