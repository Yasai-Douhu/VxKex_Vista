#ifndef KXNT_IFEO_SUITE_PROVIDER_H
#define KXNT_IFEO_SUITE_PROVIDER_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#ifdef KXNT_IFEO_RESOURCE_PHASES
/* Export is solely a symbolic debugger rendezvous in owned test images. */
__declspec(dllexport) __declspec(noinline) void __cdecl KxNtIfeoResourcePhase(unsigned phase){static volatile unsigned observed;observed=phase;}
#endif
/* Read import slots without inventing calling conventions for unrelated APIs.
   Existing probes retain their typed call signatures. No API is called here. */
#ifdef _WIN64
#define KXNT_SLOT(name) __imp_##name
#else
/* x86 C adds one leading underscore to this external variable identifier. */
#define KXNT_SLOT(name) _imp__##name
#endif
#define KXNT_BINDINGS(X) \
 X(NtAlertThreadByThreadId) X(ZwAlertThreadByThreadId) \
 X(NtWaitForAlertByThreadId) X(ZwWaitForAlertByThreadId) \
 X(RtlCanonicalizeDomainName) X(RtlCheckTokenMembershipEx) \
 X(RtlGetDeviceFamilyInfoEnum) X(RtlGetPersistedStateLocation) \
 X(RtlIsCapabilitySid) X(RtlIsPackageSid) X(RtlIsProcessorFeaturePresent) \
 X(NtCompareObjects) X(ZwCompareObjects) \
 X(NtSetInformationFile) X(ZwSetInformationFile) X(NtWriteFile) X(ZwWriteFile) \
 X(RtlQueryPerformanceCounter) X(RtlQueryPerformanceFrequency) \
 X(RtlTryAcquireSRWLockExclusive) X(RtlTryAcquireSRWLockShared) \
 X(RtlUTF8ToUnicodeN) X(RtlUnicodeToUTF8N) \
 X(NtOpenKeyEx) X(ZwOpenKeyEx) X(RtlReportSilentProcessExit)
#define KXNT_DECLARE(name) extern PVOID KXNT_SLOT(name);
KXNT_BINDINGS(KXNT_DECLARE)
#undef KXNT_DECLARE
static HMODULE KxNtIfeoLoadedProvider(const char *requested)
{
    HMODULE module=GetModuleHandleW(L"KxNt.dll"),implementation,held;
    char logpath[MAX_PATH],path[MAX_PATH];FILE *log;unsigned total=0,matches=0;
    const char *leaf=strrchr(requested,'\\');leaf=leaf?leaf+1:requested;
    /* Preserve native behavior for other library loads in the detailed tests. */
    if(_stricmp(leaf,"KxNt.dll") && _stricmp(leaf,"KexDll.dll"))return LoadLibraryA(requested);
    sprintf(logpath,"C:\\VxKexProbe\\KxNtIfeo\\bindings-%lu.txt",GetCurrentProcessId());
    log=fopen(logpath,"w");if(!log)return NULL;
    if(!module || !GetModuleHandleW(L"KexDll.dll")){fprintf(log,"EarlyKexDllLoaded=0 Result=FAIL\n");fclose(log);return NULL;}
    GetModuleFileNameA(module,path,sizeof(path));fprintf(log,"Provider=%s\n",path);
    GetModuleFileNameA(GetModuleHandleW(L"KexDll.dll"),path,sizeof(path));fprintf(log,"Implementation=%s\n",path);
#define KXNT_CHECK(name) do { \
    PVOID address=KXNT_SLOT(name);BOOL equal=address==(PVOID)GetProcAddress(module,#name); \
    char owner[MAX_PATH]=""; \
    ++total;if(equal && address)++matches; \
    if(address && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)address,&implementation))GetModuleFileNameA(implementation,owner,sizeof(owner)); \
    fprintf(log,"Import=%s Equal=%d Owner=%s\n",#name,equal && address!=NULL,owner); \
} while(0);
    KXNT_BINDINGS(KXNT_CHECK)
#undef KXNT_CHECK
    fprintf(log,"StaticBindingCount=%u Matches=%u EarlyKexDllLoaded=1 Result=%s\n",total,matches,total==matches?"PASS":"FAIL");fclose(log);
    if(total!=matches)return NULL;
    /* Acquire a reference without loading a new image: probes call FreeLibrary. */
    if(!GetModuleHandleExW(0,!_stricmp(leaf,"KexDll.dll")?L"KexDll.dll":L"KxNt.dll",&held))return NULL;
    return held;
}
#define LoadLibraryA KxNtIfeoLoadedProvider
#endif
