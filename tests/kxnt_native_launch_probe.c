#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

static void timeoutEvidence(FILE *out,PROCESS_INFORMATION *process)
{
    HANDLE snapshot;MODULEENTRY32 entry;CONTEXT context;DWORD suspended;
    ULONG_PTR stack[32];SIZE_T read=0;unsigned i;
    fprintf(out,"OwnedChildPID=%lu\n",process->dwProcessId);
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,process->dwProcessId);
    if(snapshot!=INVALID_HANDLE_VALUE){
        ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);
        if(Module32First(snapshot,&entry))do {
            fprintf(out,"ChildModule=%s Base=%p Size=%lu Path=%s\n",entry.szModule,entry.modBaseAddr,entry.modBaseSize,entry.szExePath);
        } while(Module32Next(snapshot,&entry));
        CloseHandle(snapshot);
    } else fprintf(out,"ChildModuleSnapshotError=%lu\n",GetLastError());
    suspended=SuspendThread(process->hThread);
    if(suspended!=(DWORD)-1){
        ZeroMemory(&context,sizeof(context));context.ContextFlags=CONTEXT_CONTROL;
        if(GetThreadContext(process->hThread,&context)){
#ifdef _WIN64
            ULONG_PTR pc=(ULONG_PTR)context.Rip,sp=(ULONG_PTR)context.Rsp;
#else
            ULONG_PTR pc=(ULONG_PTR)context.Eip,sp=(ULONG_PTR)context.Esp;
#endif
            fprintf(out,"ChildContext PC=%p SP=%p\n",(PVOID)pc,(PVOID)sp);
            if(ReadProcessMemory(process->hProcess,(PVOID)sp,stack,sizeof(stack),&read)){
                fprintf(out,"ChildStackBytes=%Iu",read);
                for(i=0;i<read/sizeof(stack[0]);++i)fprintf(out," %p",(PVOID)stack[i]);
                fputc('\n',out);
            } else fprintf(out,"ChildStackReadError=%lu\n",GetLastError());
        } else fprintf(out,"ChildContextError=%lu\n",GetLastError());
        ResumeThread(process->hThread);
    } else fprintf(out,"ChildSuspendError=%lu\n",GetLastError());
    fflush(out);
}

// Diagnose actual loader/runtime failure without requiring cmd.exe or a GUI.
// No compatibility DLL is loaded. The timeout only terminates our own child.
int main(int argc,char **argv)
{
    FILE *out; STARTUPINFOA startup; PROCESS_INFORMATION process;
    char command[4096]; BOOL ok; DWORD error,wait,exitCode;
    const char *names[]={"NtAlertThreadByThreadId","NtWaitForAlertByThreadId",
        "RtlQueryPerformanceCounter","RtlQueryPerformanceFrequency",
        "RtlGetSystemTimePrecise","RtlReportSilentProcessExit","NtWriteFile"};
    unsigned i;DWORD creationFlags=0;
    if(argc<3 || argc>5)return 2;
    if(argc==5){if(strcmp(argv[4],"no-console"))return 2;creationFlags=CREATE_NO_WINDOW;}
    out=fopen(argv[2],"w");if(!out)return 3;
    fprintf(out,"ProcessBits=%u KexDllLoaded=%d\n",(unsigned)(sizeof(void*)*8),GetModuleHandleW(L"KexDll.dll")!=NULL);
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
        fprintf(out,"NativeExport=%s Present=%d\n",names[i],GetProcAddress(GetModuleHandleW(L"ntdll.dll"),names[i])!=NULL);
    if(strlen(argv[1])+(argc>=4?strlen(argv[3]):0)+4>=sizeof(command))return 4;
    sprintf(command,"\"%s\" %s",argv[1],argc>=4?argv[3]:"");
    ZeroMemory(&startup,sizeof(startup));startup.cb=sizeof(startup);
    ZeroMemory(&process,sizeof(process));
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    fprintf(out,"CreationFlags=%08lx\n",creationFlags);
    fprintf(out,"CommandLine=%s\n",command);
    ok=CreateProcessA(argv[1],command,NULL,NULL,FALSE,creationFlags,NULL,NULL,&startup,&process);
    error=ok?0:GetLastError();
    fprintf(out,"CreateProcess=%d Error=%lu\n",ok,error);fflush(out);
    if(ok) {
        wait=WaitForSingleObject(process.hProcess,10000);
        if(wait==WAIT_TIMEOUT){timeoutEvidence(out,&process);TerminateProcess(process.hProcess,0xdead);WaitForSingleObject(process.hProcess,5000);}
        GetExitCodeProcess(process.hProcess,&exitCode);
        fprintf(out,"ChildWait=%08lx ChildExitCode=%08lx\n",wait,exitCode);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    fprintf(out,"Result=MEASURED\n");fclose(out);return 0;
}
