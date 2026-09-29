// Adapter for node-pty 1.2.0-beta.15, built for NT 6.0 using Node-API 8.
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include "api.h"
static HMODULE Self, Winpty;
static bool NapiReady;
static SRWLOCK InitLock=SRWLOCK_INIT;
struct Context;
struct Session {
 Context *ctx; int id; winpty_t *wp; HANDLE process,agent,thread;
 napi_threadsafe_function callback; DWORD exitCode;
 Session():ctx(NULL),id(0),wp(NULL),process(NULL),agent(NULL),thread(NULL),callback(NULL),exitCode(0){}
};
struct Context {std::vector<Session*> sessions;int next;int refs;Context():next(1),refs(1){}};
static napi_value fail(napi_env e,const char *s){p_napi_throw_error(e,NULL,s);return NULL;}
static napi_value undef(napi_env e){napi_value v;p_napi_get_undefined(e,&v);return v;}
static bool str(napi_env e,napi_value v,std::wstring &s){size_t n=0;if(p_napi_get_value_string_utf16(e,v,NULL,0,&n)!=napi_ok)return false;std::vector<char16_t> b(n+1);if(p_napi_get_value_string_utf16(e,v,&b[0],n+1,&n)!=napi_ok)return false;s.assign((wchar_t*)&b[0],n);return s.find(L'\0')==std::wstring::npos;}
static void num(napi_env e,napi_value obj,const char *key,int n){napi_value v;p_napi_create_int32(e,n,&v);p_napi_set_named_property(e,obj,key,v);}
static void text(napi_env e,napi_value obj,const char *key,LPCWSTR s){napi_value v;p_napi_create_string_utf16(e,(const char16_t*)s,wcslen(s),&v);p_napi_set_named_property(e,obj,key,v);}
static void stop(Session *s){if(s->wp){p_winpty_free(s->wp);s->wp=NULL;}}
static void destroy(Session *s){Context *c=s->ctx;stop(s);if(s->thread)CloseHandle(s->thread);if(s->process)CloseHandle(s->process);if(s->agent)CloseHandle(s->agent);c->sessions.erase(std::remove(c->sessions.begin(),c->sessions.end(),s),c->sessions.end());delete s;if(!--c->refs)delete c;}
static void finalize(napi_env,void *data,void*){destroy((Session*)data);}
static void callExit(napi_env env,napi_value cb,void*,void *data){Session *s=(Session*)data;if(env&&cb){napi_value v;p_napi_create_int32(env,(int)s->exitCode,&v);p_napi_call_function(env,undef(env),cb,1,&v,NULL);}}
static DWORD WINAPI waiter(void *arg){Session *s=(Session*)arg;WaitForSingleObject(s->process,INFINITE);GetExitCodeProcess(s->process,&s->exitCode);
 // AUTO_SHUTDOWN drains terminal output before agent exit. Bound shutdown if
 // the client stopped reading; an abandoned session cannot hold the host forever.
 WaitForSingleObject(s->agent,5000);p_napi_call_threadsafe_function(s->callback,s,napi_tsfn_nonblocking);p_napi_release_threadsafe_function(s->callback,napi_tsfn_release);return 0;}
static void cleanup(void *data){Context *c=(Context*)data;std::vector<Session*> items=c->sessions;
 for(size_t i=0;i<items.size();++i){Session *s=items[i];stop(s);if(s->process&&WaitForSingleObject(s->process,0)==WAIT_TIMEOUT)TerminateProcess(s->process,1);if(s->thread)WaitForSingleObject(s->thread,INFINITE);else if(!s->callback)destroy(s);}
 if(!--c->refs)delete c;
}
static bool args(napi_env e,napi_callback_info info,napi_value *v,size_t wanted,Context **c){size_t n=wanted;return p_napi_get_cb_info(e,info,&n,v,NULL,(void**)c)==napi_ok&&n==wanted;}
static Session *lookup(napi_env e,napi_value v,Context *c){int id;if(p_napi_get_value_int32(e,v,&id)!=napi_ok)return NULL;for(size_t i=0;i<c->sessions.size();++i)if(c->sessions[i]->id==id)return c->sessions[i];return NULL;}
static napi_value winerror(napi_env e,const char *where,winpty_error_ptr_t err){char msg[1024];LPCWSTR detail=err?p_winpty_error_msg(err):L"unknown error";char tmp[800];WideCharToMultiByte(CP_UTF8,0,detail,-1,tmp,sizeof(tmp),NULL,NULL);_snprintf_s(msg,sizeof(msg),_TRUNCATE,"%s: %s",where,tmp);if(err)p_winpty_error_free(err);return fail(e,msg);}
static napi_value start(napi_env e,napi_callback_info info){napi_value a[7];Context *c;int cols,rows;
 if(!args(e,info,a,7,&c)||p_napi_get_value_int32(e,a[1],&cols)!=napi_ok||p_napi_get_value_int32(e,a[2],&rows)!=napi_ok||cols<1||rows<1||cols>32767||rows>32767)return fail(e,"Invalid terminal dimensions");
 winpty_error_ptr_t err=NULL;winpty_config_t *cfg=p_winpty_config_new(0,&err);if(!cfg)return winerror(e,"config",err);
 p_winpty_config_set_initial_size(cfg,cols,rows);p_winpty_config_set_agent_timeout(cfg,10000);winpty_t *wp=p_winpty_open(cfg,&err);p_winpty_config_free(cfg);if(!wp)return winerror(e,"open",err);
 Session *s=new Session;s->ctx=c;s->id=c->next++;s->wp=wp;
 if(!DuplicateHandle(GetCurrentProcess(),p_winpty_agent_process(wp),GetCurrentProcess(),&s->agent,0,FALSE,DUPLICATE_SAME_ACCESS)){stop(s);delete s;return fail(e,"Cannot duplicate agent handle");}
 c->sessions.push_back(s);++c->refs;napi_value o;p_napi_create_object(e,&o);num(e,o,"pty",s->id);num(e,o,"fd",-1);text(e,o,"conin",p_winpty_conin_name(wp));text(e,o,"conout",p_winpty_conout_name(wp));return o;
}
static napi_value connectPty(napi_env e,napi_callback_info info){napi_value a[6];Context *c;std::wstring cmd,cwd;uint32_t count;
 if(!args(e,info,a,6,&c))return fail(e,"Invalid connect arguments");Session *s=lookup(e,a[0],c);
 if(!s||!s->wp||s->process||!str(e,a[1],cmd)||!str(e,a[2],cwd)||p_napi_get_array_length(e,a[3],&count)!=napi_ok)return fail(e,"Invalid terminal connection");
 napi_valuetype cbtype;p_napi_typeof(e,a[5],&cbtype);if(cbtype!=napi_function)return fail(e,"Missing exit callback");
 std::vector<wchar_t> block;for(uint32_t i=0;i<count;++i){napi_value v;std::wstring entry;p_napi_get_element(e,a[3],i,&v);if(!str(e,v,entry))return fail(e,"Invalid environment string");block.insert(block.end(),entry.begin(),entry.end());block.push_back(0);}block.push_back(0);if(!count)block.push_back(0);
 winpty_error_ptr_t err=NULL;winpty_spawn_config_t *cfg=p_winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN|WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN,NULL,cmd.c_str(),cwd.c_str(),&block[0],&err);if(!cfg)return winerror(e,"spawn config",err);
 DWORD error=0;BOOL ok=p_winpty_spawn(s->wp,cfg,&s->process,NULL,&error,&err);p_winpty_spawn_config_free(cfg);if(!ok)return winerror(e,"spawn",err);
 napi_value name;p_napi_create_string_utf8(e,"VistaPty exit",NAPI_AUTO_LENGTH,&name);
 if(p_napi_create_threadsafe_function(e,a[5],NULL,name,1,1,s,finalize,NULL,callExit,&s->callback)!=napi_ok){stop(s);destroy(s);return fail(e,"Cannot create exit callback");}
 s->thread=CreateThread(NULL,0,waiter,s,0,NULL);if(!s->thread){stop(s);p_napi_release_threadsafe_function(s->callback,napi_tsfn_abort);return fail(e,"Cannot create exit waiter");}
 napi_value o;p_napi_create_object(e,&o);num(e,o,"pid",GetProcessId(s->process));return o;
}
static napi_value resize(napi_env e,napi_callback_info info){napi_value a[4];Context *c;int cols,rows;if(!args(e,info,a,4,&c))return fail(e,"Invalid resize arguments");Session *s=lookup(e,a[0],c);if(!s||!s->wp)return undef(e);if(p_napi_get_value_int32(e,a[1],&cols)!=napi_ok||p_napi_get_value_int32(e,a[2],&rows)!=napi_ok||cols<1||rows<1||cols>32767||rows>32767)return fail(e,"Invalid terminal dimensions");winpty_error_ptr_t err=NULL;if(!p_winpty_set_size(s->wp,cols,rows,&err))return winerror(e,"resize",err);return undef(e);}
static napi_value killPty(napi_env e,napi_callback_info info){napi_value a[2];Context *c;if(!args(e,info,a,2,&c))return fail(e,"Invalid kill arguments");Session *s=lookup(e,a[0],c);if(s){stop(s);if(!s->callback)destroy(s);}return undef(e);}
static napi_value clearPty(napi_env e,napi_callback_info info){napi_value a[2];Context *c;if(!args(e,info,a,2,&c))return fail(e,"Invalid clear arguments");Session *s=lookup(e,a[0],c);if(!s||!s->process)return undef(e);
 WCHAR path[MAX_PATH],cmd[2*MAX_PATH];GetModuleFileNameW(Self,path,MAX_PATH);wchar_t *tail=wcsrchr(path,L'\\');if(!tail)return fail(e,"Invalid helper path");wcscpy_s(tail,MAX_PATH-(tail-path),L"\\vista-pty-clear.exe");_snwprintf_s(cmd,2*MAX_PATH,_TRUNCATE,L"\"%s\" %lu",path,GetProcessId(s->process));STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;
 if(!CreateProcessW(path,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi))return fail(e,"Cannot start terminal clear helper");CloseHandle(pi.hThread);DWORD wait=WaitForSingleObject(pi.hProcess,3000),ec=1;if(wait==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,1);else GetExitCodeProcess(pi.hProcess,&ec);CloseHandle(pi.hProcess);if(ec)return fail(e,"Terminal clear failed");return undef(e);
}
extern "C" __declspec(dllexport) napi_value napi_register_module_v1(napi_env e,napi_value exports){HMODULE host=GetModuleHandleW(NULL);
 AcquireSRWLockExclusive(&InitLock);
#define LOAD_NAPI(n) p_##n=(decltype(&n))GetProcAddress(host,#n);if(!p_##n){ReleaseSRWLockExclusive(&InitLock);return NULL;}
 if(!NapiReady){NAPI_LIST(LOAD_NAPI) NapiReady=true;}
 if(!Winpty){WCHAR path[MAX_PATH];GetModuleFileNameW(Self,path,MAX_PATH);wchar_t *tail=wcsrchr(path,L'\\');if(tail){wcscpy_s(tail,MAX_PATH-(tail-path),L"\\winpty.dll");Winpty=LoadLibraryW(path);}}
 bool loaded=Winpty!=NULL;
#define LOAD_WINPTY(n) p_##n=(decltype(&n))GetProcAddress(Winpty,#n);if(!p_##n)loaded=false;
 if(loaded&&!p_winpty_open){WINPTY_LIST(LOAD_WINPTY)}ReleaseSRWLockExclusive(&InitLock);
 if(!loaded)return fail(e,"VistaPty cannot load adjacent winpty.dll");Context *c=new Context;p_napi_add_env_cleanup_hook(e,cleanup,c);
 const char *names[]={"startProcess","connect","resize","kill","clear"};napi_callback callbacks[]={start,connectPty,resize,killPty,clearPty};for(int i=0;i<5;++i){napi_value f;p_napi_create_function(e,names[i],NAPI_AUTO_LENGTH,callbacks[i],c,&f);p_napi_set_named_property(e,exports,names[i],f);}text(e,exports,"backend",L"VxKex Vista WinPTY");return exports;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){Self=h;DisableThreadLibraryCalls(h);}return TRUE;}
