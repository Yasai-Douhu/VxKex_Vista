#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <sddl.h>
#include <objbase.h>

DWORD _tls_index = 0;
static int failures;
static SRWLOCK lock = SRWLOCK_INIT;
static LONG counter;
static BOOLEAN (WINAPI *tryShared)(PSRWLOCK);
static void check(const char *name, BOOL ok) {
    char text[256]; DWORD written;
    wsprintfA(text, "%s %s\r\n", ok ? "PASS" : "FAIL", name);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, lstrlenA(text), &written, NULL);
    if (!ok) ++failures;
}
static DWORD WINAPI worker(void *unused) {
    int i;
    for (i = 0; i < 10000; ++i) {
        AcquireSRWLockExclusive(&lock);
        ++counter;
        ReleaseSRWLockExclusive(&lock);
        while (!tryShared(&lock)) SwitchToThread();
        ReleaseSRWLockShared(&lock);
    }
    return 0;
}
void mainCRTStartup(void) {
    HMODULE base = LoadLibraryW(L"KxBase.dll"), user = LoadLibraryW(L"KxUser.dll");
    BOOL (WINAPI *affinity)(HANDLE, PGROUP_AFFINITY);
    HANDLE (WINAPI *power)(PVOID);
    int (WINAPI *locale)(LPCWSTR, LPWSTR, int);
    HRESULT (WINAPI *setId)(PCWSTR);
    HRESULT (WINAPI *getId)(PWSTR *);
    DWORD (WINAPI *moduleNameW)(HANDLE, HMODULE, LPWSTR, DWORD);
    DWORD (WINAPI *moduleNameA)(HANDLE, HMODULE, LPSTR, DWORD);
    CHAR ansiName[MAX_PATH];
    BOOL (WINAPI *attribute)(LPPROC_THREAD_ATTRIBUTE_LIST,DWORD,DWORD_PTR,PVOID,SIZE_T,PVOID,PSIZE_T);
    GROUP_AFFINITY group; WCHAR name[85], *id = NULL;
    HANDLE threads[4], mapping, readOnly = NULL, upgraded = NULL;
    PSECURITY_DESCRIPTOR descriptor = NULL; SECURITY_ATTRIBUTES sa;
    SIZE_T bytes = 0; LPPROC_THREAD_ATTRIBUTE_LIST list; DWORD policy = 0; int i; BOOL ok;
    check("load compatibility DLLs", base && user);
    if (!base || !user) ExitProcess(1);
    tryShared = (void *) GetProcAddress(base, "TryAcquireSRWLockShared");
    affinity = (void *) GetProcAddress(base, "GetThreadGroupAffinity");
    power = (void *) GetProcAddress(base, "PowerCreateRequest");
    locale = (void *) GetProcAddress(base, "ResolveLocaleName");
    attribute = (void *) GetProcAddress(base, "UpdateProcThreadAttribute");
    setId = (void *) GetProcAddress(user, "SetCurrentProcessExplicitAppUserModelID");
    getId = (void *) GetProcAddress(user, "GetCurrentProcessExplicitAppUserModelID");
    check("required exports", tryShared && affinity && power && locale && attribute && setId && getId);
    if (failures) ExitProcess(1);
    moduleNameW = (void *) GetProcAddress(base, "K32GetModuleBaseNameW");
    moduleNameA = (void *) GetProcAddress(base, "K32GetModuleBaseNameA");
    check("K32 module name Unicode", moduleNameW && moduleNameW(GetCurrentProcess(),NULL,name,85)>0 && lstrcmpiW(name,L"vista_compat_probe.exe")==0);
    check("K32 module name ANSI", moduleNameA && moduleNameA(GetCurrentProcess(),NULL,ansiName,MAX_PATH)>0 && lstrcmpiA(ansiName,"vista_compat_probe.exe")==0);
    check("first shared acquisition", tryShared(&lock));
    check("second shared acquisition", tryShared(&lock));
    ReleaseSRWLockShared(&lock); ReleaseSRWLockShared(&lock);
    check("native release clears shared lock", lock.Ptr == NULL);
    AcquireSRWLockExclusive(&lock);
    check("exclusive owner rejects shared try", !tryShared(&lock));
    ReleaseSRWLockExclusive(&lock);
    AcquireSRWLockShared(&lock);
    check("interop with native shared acquire", tryShared(&lock));
    ReleaseSRWLockShared(&lock); ReleaseSRWLockShared(&lock);
    for (i=0; i<4; ++i) threads[i]=CreateThread(NULL,0,worker,NULL,0,NULL);
    check("mixed lock contention completes", WaitForMultipleObjects(4,threads,TRUE,30000)==WAIT_OBJECT_0 && counter==40000);
    for (i=0; i<4; ++i) CloseHandle(threads[i]);
    check("thread affinity group zero", affinity(GetCurrentThread(),&group) && group.Group==0 && group.Mask!=0);
    check("thread affinity null rejected", !affinity(GetCurrentThread(),NULL) && GetLastError()==ERROR_INVALID_PARAMETER);
    check("power requests report unsupported", power(NULL)==INVALID_HANDLE_VALUE && GetLastError()==ERROR_NOT_SUPPORTED);
    check("locale en-US roundtrip", locale(L"en-US",name,85)>0 && lstrcmpW(name,L"en-US")==0);
    check("locale short buffer", locale(L"en-US",name,1)==0 && GetLastError()==ERROR_INSUFFICIENT_BUFFER);
    check("application ID stored", setId(L"VxKex.Vista.Probe")==S_OK);
    check("application ID roundtrip", getId(&id)==S_OK && id && lstrcmpW(id,L"VxKex.Vista.Probe")==0);
    CoTaskMemFree(id);
    InitializeProcThreadAttributeList(NULL,1,0,&bytes);
    list=HeapAlloc(GetProcessHeap(),0,bytes);
    check("attribute list initialized", InitializeProcThreadAttributeList(list,1,0,&bytes));
    check("empty mitigation accepted", attribute(list,0,PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,&policy,sizeof(policy),NULL,NULL));
    DeleteProcThreadAttributeList(list); HeapFree(GetProcessHeap(),0,list);
    // An unnamed section's read-only DACL must survive handle duplication.
    ok=ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GR;;;WD)",SDDL_REVISION_1,&descriptor,NULL);
    check("section security descriptor",ok);
    if (ok) {
        sa.nLength=sizeof(sa);sa.lpSecurityDescriptor=descriptor;sa.bInheritHandle=FALSE;
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,4096,NULL);
        check("create section with DACL",mapping!=NULL);
        if (mapping) {
            ok=DuplicateHandle(GetCurrentProcess(),mapping,GetCurrentProcess(),&readOnly,FILE_MAP_READ,FALSE,0);
            check("duplicate section read-only",ok);
            if (ok) {
                ok=DuplicateHandle(GetCurrentProcess(),readOnly,GetCurrentProcess(),&upgraded,FILE_MAP_WRITE,FALSE,0);
                check("read-only section cannot gain write access",!ok && GetLastError()==ERROR_ACCESS_DENIED);
                if (ok) CloseHandle(upgraded);
                CloseHandle(readOnly);
            }
            CloseHandle(mapping);
        }
        LocalFree(descriptor);
    }
    ExitProcess(failures ? 1 : 0);
}
