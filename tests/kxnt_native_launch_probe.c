#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

// Diagnose actual loader/runtime failure without requiring cmd.exe or a GUI.
// No compatibility DLL is loaded. The timeout only terminates our own child.
int main(int argc,char **argv)
{
    FILE *out; STARTUPINFOA startup; PROCESS_INFORMATION process;
    char command[4096]; BOOL ok; DWORD error,wait,exitCode;
    const char *names[]={"NtAlertThreadByThreadId","NtWaitForAlertByThreadId",
        "RtlQueryPerformanceCounter","RtlQueryPerformanceFrequency",
        "RtlGetSystemTimePrecise","RtlReportSilentProcessExit","NtWriteFile"};
    unsigned i;
    if(argc<3 || argc>4)return 2;
    out=fopen(argv[2],"w");if(!out)return 3;
    fprintf(out,"ProcessBits=%u KexDllLoaded=%d\n",(unsigned)(sizeof(void*)*8),GetModuleHandleW(L"KexDll.dll")!=NULL);
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
        fprintf(out,"NativeExport=%s Present=%d\n",names[i],GetProcAddress(GetModuleHandleW(L"ntdll.dll"),names[i])!=NULL);
    if(strlen(argv[1])+(argc==4?strlen(argv[3]):0)+4>=sizeof(command))return 4;
    sprintf(command,"\"%s\" %s",argv[1],argc==4?argv[3]:"");
    ZeroMemory(&startup,sizeof(startup));startup.cb=sizeof(startup);
    ZeroMemory(&process,sizeof(process));
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    ok=CreateProcessA(argv[1],command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&process);
    error=ok?0:GetLastError();
    fprintf(out,"CreateProcess=%d Error=%lu\n",ok,error);fflush(out);
    if(ok) {
        wait=WaitForSingleObject(process.hProcess,10000);
        if(wait==WAIT_TIMEOUT){TerminateProcess(process.hProcess,0xdead);WaitForSingleObject(process.hProcess,5000);}
        GetExitCodeProcess(process.hProcess,&exitCode);
        fprintf(out,"ChildWait=%08lx ChildExitCode=%08lx\n",wait,exitCode);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    fprintf(out,"Result=MEASURED\n");fclose(out);return 0;
}
