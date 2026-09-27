// Run interactively on NT 6.0. No persistent settings are changed.
#include <windows.h>
#include <tlhelp32.h>
static STARTUPINFOEXW si;
static PROCESS_INFORMATION pi;
static SECURITY_ATTRIBUTES sa;
typedef BOOL (WINAPI *UPDATE)(LPPROC_THREAD_ATTRIBUTE_LIST,DWORD,DWORD_PTR,PVOID,SIZE_T,PVOID,PSIZE_T);
static HANDLE log;
static MODULEENTRY32W module;
static void line(char *s){DWORD n;WriteFile(log,s,lstrlenA(s),&n,0);}
void mainCRTStartup(void){
 WCHAR cmd[1024],path[MAX_PATH]; char text[256]; HANDLE rd,wr,handles[2],con; SIZE_T size=0; UPDATE update; BOOL ok; DWORD error,exitcode; HMODULE base;
 log=CreateFileW(L"C:\\VxKexProbe\\handle-list-probe.txt",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,0,0);
 if(wcsstr(GetCommandLineW(),L"--child")){
  HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);DWORD type=GetFileType(out);
  wsprintfA(text,"child stdout=%p type=%lu\r\n",out,type);line(text);CloseHandle(log);ExitProcess(type==FILE_TYPE_PIPE?0:1);
 }
 FreeConsole();if(!AttachConsole(1948)){line("AttachConsole failed\r\n");ExitProcess(2);}
 sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
 CreatePipe(&rd,&wr,&sa,0);con=CreateFileW(L"CONOUT$",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,0);
 handles[0]=wr;handles[1]=con;
 wsprintfA(text,"handles pipe=%p console=%p consoleType=%lu\r\n",wr,con,GetFileType(con));line(text);
 InitializeProcThreadAttributeList(0,1,0,&size);
 si.lpAttributeList=HeapAlloc(GetProcessHeap(),0,size);
 InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size);
 {HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());module.dwSize=sizeof(module);update=0;
 if(Module32FirstW(snapshot,&module))do{if(!lstrcmpiW(module.szModule,L"kernel32.dll")){update=(UPDATE)GetProcAddress(module.hModule,"UpdateProcThreadAttribute");break;}}while(Module32NextW(snapshot,&module));CloseHandle(snapshot);}
 if(!update){line("Cannot resolve native API\r\n");ExitProcess(5);}
 SetLastError(0);ok=update(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),0,0);error=GetLastError();
 wsprintfA(text,"native mixed-list update=%d error=%lu\r\n",ok,error);line(text);
 si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;si.StartupInfo.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.StartupInfo.hStdOutput=wr;si.StartupInfo.hStdError=con;
 GetModuleFileNameW(0,path,MAX_PATH);wsprintfW(cmd,L"\"%s\" --child",path);
 if(CreateProcessW(0,cmd,0,0,TRUE,EXTENDED_STARTUPINFO_PRESENT,0,0,&si.StartupInfo,&pi)){
  WaitForSingleObject(pi.hProcess,30000);GetExitCodeProcess(pi.hProcess,&exitcode);wsprintfA(text,"native child exit=%lu\r\n",exitcode);line(text);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }
 DeleteProcThreadAttributeList(si.lpAttributeList);InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size);
 base=LoadLibraryW(L"C:\\VxKex\\KxBase.dll");update=(UPDATE)GetProcAddress(base,"UpdateProcThreadAttribute");
 if(!update){line("Cannot resolve compatibility API\r\n");ExitProcess(3);}
 ok=update(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),0,0);error=GetLastError();
 wsprintfA(text,"compat mixed-list update=%d error=%lu\r\n",ok,error);line(text);
 DeleteProcThreadAttributeList(si.lpAttributeList);InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size);
 SetLastError(0);ok=update(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(HANDLE),0,0);error=GetLastError();
 wsprintfA(text,"compat pipe-only update=%d error=%lu\r\n",ok,error);line(text);
 DeleteProcThreadAttributeList(si.lpAttributeList);HeapFree(GetProcessHeap(),0,si.lpAttributeList);CloseHandle(rd);CloseHandle(wr);CloseHandle(con);CloseHandle(log);ExitProcess(ok?0:4);
}
