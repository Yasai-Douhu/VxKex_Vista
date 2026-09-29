#include <windows.h>
#include <stdio.h>
#include "winpty.h"
#define API(n) decltype(&n) p_##n=(decltype(&n))GetProcAddress(dll,#n); if(!p_##n){fprintf(f,"Missing %s\n",#n);return 3;}
int wmain(int argc,wchar_t **argv) {
 if(argc!=3)return 2;FILE *f=_wfopen(argv[2],L"w");if(!f)return 2;
 fprintf(f,"KexLoaded=%d\n",GetModuleHandleW(L"KexDll.dll")!=NULL);fflush(f);
 if(wcscmp(argv[1],L"winpty")) {
  fprintf(f,"KernelCreatePseudoConsole=%p\n",GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreatePseudoConsole"));
  HMODULE dll=LoadLibraryW(argv[1]);fprintf(f,"ConptyLoad=%p error=%lu\n",dll,GetLastError());fflush(f);
  if(!dll){fclose(f);return 1;}
  typedef HRESULT(WINAPI *CREATE)(COORD,HANDLE,HANDLE,DWORD,void**);
  CREATE create=(CREATE)GetProcAddress(dll,"ConptyCreatePseudoConsole");
  HANDLE ir,iw,orr,ow;CreatePipe(&ir,&iw,NULL,0);CreatePipe(&orr,&ow,NULL,0);void *pc=NULL;COORD sz={80,25};
  HRESULT hr=create?create(sz,ir,ow,0,&pc):E_NOTIMPL;fprintf(f,"ConptyCreate=%08lx lastError=%lu\n",hr,GetLastError());
  if(pc){typedef void(WINAPI *CLOSE)(void*);((CLOSE)GetProcAddress(dll,"ConptyClosePseudoConsole"))(pc);}
  CloseHandle(ir);CloseHandle(iw);CloseHandle(orr);CloseHandle(ow);fclose(f);return FAILED(hr)?1:0;
 }
 HMODULE dll=LoadLibraryW(L"winpty.dll");fprintf(f,"WinptyLoad=%p error=%lu\n",dll,GetLastError());if(!dll){fclose(f);return 1;}
 API(winpty_config_new);API(winpty_config_free);API(winpty_config_set_initial_size);API(winpty_config_set_agent_timeout);
 API(winpty_open);API(winpty_conin_name);API(winpty_conout_name);API(winpty_spawn_config_new);API(winpty_spawn_config_free);
 API(winpty_spawn);API(winpty_set_size);API(winpty_free);API(winpty_error_msg);API(winpty_error_free);
 winpty_error_ptr_t err=NULL;winpty_config_t *cfg=p_winpty_config_new(0,&err);
 p_winpty_config_set_initial_size(cfg,80,25);p_winpty_config_set_agent_timeout(cfg,10000);
 winpty_t *wp=p_winpty_open(cfg,&err);p_winpty_config_free(cfg);
 if(!wp){fwprintf(f,L"Open failed: %s\n",err?p_winpty_error_msg(err):L"unknown");if(err)p_winpty_error_free(err);fclose(f);return 1;}
 HANDLE in=CreateFileW(p_winpty_conin_name(wp),GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
 HANDLE out=CreateFileW(p_winpty_conout_name(wp),GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
 winpty_spawn_config_t *sc=p_winpty_spawn_config_new(1,L"C:\\Windows\\System32\\cmd.exe",L"cmd.exe /d /q",L"C:\\VxKexProbe",NULL,&err);
 HANDLE proc=NULL;DWORD ce=0;BOOL ok=p_winpty_spawn(wp,sc,&proc,NULL,&ce,&err);p_winpty_spawn_config_free(sc);
 fprintf(f,"Spawn=%d error=%lu\n",ok,ce);fflush(f);
 if(!ok){p_winpty_free(wp);fclose(f);return 1;}
 BOOL resized=p_winpty_set_size(wp,100,30,&err);fprintf(f,"Resize=%d\n",resized);
 const char *cmd="echo WINPTY_INTERACTIVE_OK\r\nexit /b 7\r\n";DWORD written;BOOL sent=WriteFile(in,cmd,(DWORD)strlen(cmd),&written,NULL);
 fprintf(f,"Write=%d bytes=%lu\n",sent,written);
 char all[16384]={0};DWORD used=0,start=GetTickCount();
 while(GetTickCount()-start<10000 && used<sizeof(all)-1){DWORD avail=0,n=0;if(!PeekNamedPipe(out,NULL,0,NULL,&avail,NULL))break;
  if(avail){DWORD cap=(DWORD)sizeof(all)-1-used;if(ReadFile(out,all+used,min(avail,cap),&n,NULL)){used+=n;all[used]=0;}}
  else Sleep(20);
 }
 DWORD wait=WaitForSingleObject(proc,3000),ec=0;GetExitCodeProcess(proc,&ec);
 fprintf(f,"OutputBytes=%lu\n%s\nWait=%lu ExitCode=%lu\n",used,all,wait,ec);
 bool pass=resized&&sent&&strstr(all,"WINPTY_INTERACTIVE_OK")&&wait==0&&ec==7;
 CloseHandle(proc);CloseHandle(in);CloseHandle(out);p_winpty_free(wp);fprintf(f,"PASS=%d\n",pass);fclose(f);return pass?0:1;
}
