#include "../VxlView/vxlview.h"
#include "../VxlView/backendp.h"
#include <stdio.h>
#ifdef _WIN64
#define ARCH L"x64"
#else
#define ARCH L"x86"
#endif
#define BASE L"C:\\VxKexProbe\\NextParity\\"
static FILE *log; static unsigned failures, dialogs;
static BOOL failHeap, failThread; static HANDLE worker;
static WCHAR message[1024];
typedef PVOID (NTAPI *ALLOC)(HANDLE,ULONG,SIZE_T);
typedef NTSTATUS (NTAPI *START)(HANDLE,PSECURITY_DESCRIPTOR,BOOLEAN,ULONG,SIZE_T,SIZE_T,PUSER_THREAD_START_ROUTINE,PVOID,PHANDLE,PCLIENT_ID);
static ALLOC originalAlloc; static START originalStart;
static void check(BOOL ok, PCSTR text) { fprintf(log,"%s %s\n",ok?"PASS":"FAIL",text); fflush(log); if(!ok) ++failures; }
static PVOID NTAPI alloc(HANDLE heap,ULONG flags,SIZE_T size) {
    if(failHeap) { failHeap=FALSE; return NULL; } return originalAlloc(heap,flags,size);
}
static NTSTATUS NTAPI start(HANDLE process,PSECURITY_DESCRIPTOR sd,BOOLEAN suspended,ULONG zero,SIZE_T maximum,SIZE_T committed,PUSER_THREAD_START_ROUTINE entry,PVOID parameter,PHANDLE thread,PCLIENT_ID client) {
    if(failThread) return STATUS_NO_MEMORY;
    return originalStart(process,sd,TRUE,zero,maximum,committed,entry,parameter,&worker,client);
}
static HRESULT WINAPI dialog(const TASKDIALOGCONFIG *config,int *button,int *radio,BOOL *verification) {
    ++dialogs; StringCchCopy(message,ARRAYSIZE(message),config->pszContent ? config->pszContent : L"");
    if(button) *button=IDOK; return S_OK;
}
static PVOID *slot(PCSTR name) {
    PBYTE base=(PBYTE)GetModuleHandle(NULL); PIMAGE_NT_HEADERS nt=(PIMAGE_NT_HEADERS)(base+((PIMAGE_DOS_HEADER)base)->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR import=(PIMAGE_IMPORT_DESCRIPTOR)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;import->Name;++import) {
        PIMAGE_THUNK_DATA names=(PIMAGE_THUNK_DATA)(base+import->OriginalFirstThunk), addresses=(PIMAGE_THUNK_DATA)(base+import->FirstThunk);
        if(!import->OriginalFirstThunk) continue;
        for(;names->u1.AddressOfData;++names,++addresses) {
            if(!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal) && !strcmp((PCSTR)((PIMAGE_IMPORT_BY_NAME)(base+names->u1.AddressOfData))->Name,name)) return (PVOID*)&addresses->u1.Function;
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal) && (PVOID)addresses->u1.Function ==
                (PVOID)GetProcAddress(GetModuleHandleA((PCSTR)(base+import->Name)),name)) return (PVOID*)&addresses->u1.Function;
        }
    }
    return NULL;
}
static BOOL replace(PVOID *address,PVOID value) {
    DWORD old,ignored; if(!address || !VirtualProtect(address,sizeof(*address),PAGE_READWRITE,&old)) return FALSE;
    *address=value; return VirtualProtect(address,sizeof(*address),old,&ignored);
}
static BOOL waitWorker(void) {
    MSG msg; DWORD result; ULONGLONG until=GetTickCount64()+10000;
    while(GetTickCount64()<until) {
        result=MsgWaitForMultipleObjects(1,&worker,FALSE,100,QS_ALLINPUT);
        if(result==WAIT_OBJECT_0) return TRUE;
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    return FALSE;
}
static void exportCase(PCWSTR path,NTSTATUS expected) {
    WCHAR transient[MAX_PATH]; DWORD code=0; unsigned before=dialogs;
    StringCchCopy(transient,ARRAYSIZE(transient),path); worker=NULL;
    ExportLog(transient); check(worker && !IsWindowEnabled(MainWindow),"async export disables owner before worker runs");
    if(!worker) return;
    // The real worker is suspended by our own import wrapper to make lifetime deterministic.
    StringCchCopy(transient,ARRAYSIZE(transient),L"Z:\\mutated-stack-name.txt");
    ResumeThread(worker); check(waitWorker(),"real worker terminates while UI messages are pumped");
    GetExitCodeThread(worker,&code); CloseHandle(worker); worker=NULL;
    fprintf(log,"WorkerStatus=%08lx Expected=%08lx Dialog=%ls\n",code,expected,message);
    check(code==(DWORD)expected && dialogs==before+1,"worker reports actual success or failure once");
    check(IsWindowEnabled(MainWindow) && (HCURSOR)GetClassLongPtr(MainWindow,GCLP_HCURSOR)==LoadCursor(NULL,IDC_ARROW),"owner and cursor restored after export");
    check(GetLogEntryRaw(5)!=NULL,"opened log remains usable after export");
}
VOID __cdecl ExportFailureEntry(VOID) {
    WCHAR path[MAX_PATH],name[MAX_PATH],stationName[80]; UNICODE_STRING ntName; OBJECT_ATTRIBUTES attributes; ULONG count=0,size=sizeof(count);
    HWINSTA originalStation,station; HDESK originalDesktop,desktop; WNDCLASS wc={0}; NTSTATUS status; HANDLE locked;
    PVOID *heapSlot,*threadSlot,*dialogSlot,originalDialog; unsigned before;
    StringCchPrintf(path,ARRAYSIZE(path),BASE L"vxl-export-failure-" ARCH L".txt"); log=_wfopen(path,L"wt"); if(!log) ExitProcess(2); setbuf(log,NULL);
    originalStation=GetProcessWindowStation(); originalDesktop=GetThreadDesktop(GetCurrentThreadId());
    StringCchPrintf(stationName,ARRAYSIZE(stationName),L"VxlExportProbe%lu",GetCurrentProcessId());
    station=CreateWindowStation(stationName,0,WINSTA_ALL_ACCESS,NULL); if(!station || !SetProcessWindowStation(station)) ExitProcess(3);
    desktop=CreateDesktop(L"Default",NULL,NULL,0,GENERIC_ALL,NULL); if(!desktop || !SetThreadDesktop(desktop)) ExitProcess(4);
    wc.lpfnWndProc=DefWindowProc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName=L"VxlExportProbeWindow"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
    UNCONST(HWND) MainWindow=CreateWindow(wc.lpszClassName,L"Owned export probe",WS_OVERLAPPEDWINDOW,0,0,500,300,NULL,NULL,wc.hInstance,NULL);
    UNCONST(HWND) StatusBarWindow=CreateWindow(L"STATIC",L"",WS_CHILD,0,0,100,20,MainWindow,NULL,wc.hInstance,NULL);
    check(MainWindow && StatusBarWindow,"create owned private export UI"); InitializeBackend();
    StringCchPrintf(name,ARRAYSIZE(name),L"\\??\\" BASE L"viewer-reader-fixture-" ARCH L".vxl"); RtlInitUnicodeString(&ntName,name); InitializeObjectAttributes(&attributes,&ntName,OBJ_CASE_INSENSITIVE,NULL,NULL);
    status=VxlOpenLog(&State->LogHandle,NULL,&attributes,GENERIC_READ,FILE_OPEN); check(NT_SUCCESS(status),"open real six-entry log with fixed reader"); if(!NT_SUCCESS(status)) ExitProcess(5);
    status=VxlQueryInformationLog(State->LogHandle,LogTotalNumberOfEvents,&count,&size); check(NT_SUCCESS(status)&&count==6,"query six entries"); State->NumberOfLogEntries=count; State->LogEntryCache=SafeAlloc(PLOGENTRYCACHEENTRY,count); ZeroMemory(State->LogEntryCache,count*sizeof(PLOGENTRYCACHEENTRY));
    heapSlot=slot("RtlAllocateHeap"); threadSlot=slot("RtlCreateUserThread"); dialogSlot=slot("TaskDialogIndirect");
    check(heapSlot && threadSlot && dialogSlot,"resolve imports only in owned diagnostic executable"); if(failures) ExitProcess(6);
    originalAlloc=(ALLOC)*heapSlot; originalStart=(START)*threadSlot; originalDialog=*dialogSlot;
    check(replace(heapSlot,alloc)&&replace(threadSlot,start)&&replace(dialogSlot,dialog),"install owned fault wrappers, production source has no test switches");
    StringCchPrintf(path,ARRAYSIZE(path),BASE L"vxl-async-output-" ARCH L".txt");
    check(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES,"never overwrite a preexisting success fixture"); if(failures) ExitProcess(7);
    before=dialogs; failHeap=TRUE; ExportLog(path); check(!failHeap && dialogs==before+1 && IsWindowEnabled(MainWindow),"allocation failure reports error and keeps owner usable");
    before=dialogs; failThread=TRUE; ExportLog(path); failThread=FALSE; check(dialogs==before+1 && IsWindowEnabled(MainWindow) && (HCURSOR)GetClassLongPtr(MainWindow,GCLP_HCURSOR)==LoadCursor(NULL,IDC_ARROW),"thread creation failure restores owner and cursor");
    StringCchPrintf(name,ARRAYSIZE(name),BASE L"AbsentAsyncDirectory-261001\\output.txt"); check(GetFileAttributes(BASE L"AbsentAsyncDirectory-261001")==INVALID_FILE_ATTRIBUTES,"missing-directory fixture absent"); exportCase(name,STATUS_OBJECT_PATH_NOT_FOUND);
    locked=CreateFile(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL); check(locked!=INVALID_HANDLE_VALUE,"create owned exclusively locked output");
    if(locked!=INVALID_HANDLE_VALUE) { exportCase(path,STATUS_SHARING_VIOLATION); CloseHandle(locked); check(DeleteFile(path),"remove owned locked output"); }
    exportCase(path,STATUS_SUCCESS);
    { HANDLE file=CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); WCHAR text[8192]={0}; DWORD bytes=0;
      check(file!=INVALID_HANDLE_VALUE && ReadFile(file,text,sizeof(text)-2,&bytes,NULL) && bytes>2 && text[0]==0xfeff && wcsstr(text,L"record 0") && wcsstr(text,L"record 5") && wcsstr(text,L"\x65e5\x672c\x8a9e 5"),"real async export has BOM, all endpoint entries and Japanese body despite caller buffer mutation");
      if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); check(DeleteFile(path),"remove owned successful output"); }
    check(replace(heapSlot,originalAlloc)&&replace(threadSlot,originalStart)&&replace(dialogSlot,originalDialog),"restore owned imports"); CleanupBackend(); DestroyWindow(MainWindow);
    SetThreadDesktop(originalDesktop); SetProcessWindowStation(originalStation); CloseDesktop(desktop); CloseWindowStation(station);
    fprintf(log,"Failures=%u\n",failures); fclose(log); ExitProcess(failures?1:0);
}
