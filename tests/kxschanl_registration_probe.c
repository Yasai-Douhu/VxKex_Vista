#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <windows.h>
#include <sspi.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "secur32.lib")

int main(int argc, char **argv)
{
#if defined(_M_IX86)
    const char *log = "C:\\VxKexProbe\\NextParity\\registration-x86.txt";
#else
    const char *log = "C:\\VxKexProbe\\NextParity\\registration-x64.txt";
#endif
    FILE *output = fopen(log, "w");
    PSecPkgInfoW packages = NULL;
    ULONG count = 0, index;
    BOOL found = FALSE;
    BOOL credssp = FALSE, native = argc == 2 && !strcmp(argv[1], "--native");
    SECURITY_STATUS acquired = SEC_E_INTERNAL_ERROR, freed = SEC_E_INTERNAL_ERROR;
    SECURITY_STATUS status;
    if (!output) return 2;
    setbuf(output, NULL);
    status = EnumerateSecurityPackagesW(&count, &packages);
    fprintf(output, "EnumerateSecurityPackagesW=0x%08lx count=%lu\n",
        (unsigned long) status, count);
    if (status == SEC_E_OK && packages) {
        for (index = 0; index < count; ++index) {
            char name[128] = {0};
            if (packages[index].Name) {
                WideCharToMultiByte(CP_ACP, 0, packages[index].Name, -1,
                    name, sizeof(name), NULL, NULL);
            }
            fprintf(output, "Package[%lu]=%s\n", index, name);
            if (packages[index].Name &&
                _wcsicmp(packages[index].Name, L"KxSChanl") == 0) {
                found = TRUE;
            }
            if (packages[index].Name && !_wcsicmp(packages[index].Name, L"CredSSP")) credssp = TRUE;
        }
        FreeContextBuffer(packages);
    }
    fprintf(output, "KxSChanlRegistered=%d Loaded=%d\n", found,
        GetModuleHandleW(L"KxSChanl.dll") != NULL);
    fprintf(output, "KexDllLoaded=%d KxAdvapiLoaded=%d\n",
        GetModuleHandleW(L"KexDll.dll") != NULL,
        GetModuleHandleW(L"KxAdvapi.dll") != NULL);
    if (found) {
        CredHandle credential;
        TimeStamp expiry;
        acquired = AcquireCredentialsHandleW(NULL,
            L"KxSChanl", SECPKG_CRED_OUTBOUND, NULL, NULL, NULL, NULL,
            &credential, &expiry);
        fprintf(output, "AcquireCredentialsHandleW=0x%08lx\n",
            (unsigned long) acquired);
        if (acquired == SEC_E_OK) {
            freed = FreeCredentialsHandle(&credential);
            fprintf(output, "FreeCredentialsHandle=0x%08lx\n",
                (unsigned long) freed);
        }
    }
    {
        BOOL passed = status == SEC_E_OK && credssp && (native ?
            !found && !GetModuleHandleW(L"KxSChanl.dll") && !GetModuleHandleW(L"KexDll.dll") :
            found && GetModuleHandleW(L"KxSChanl.dll") && GetModuleHandleW(L"KexDll.dll") &&
            GetModuleHandleW(L"KxAdvapi.dll") && acquired == SEC_E_OK && freed == SEC_E_OK);
        fprintf(output, "NativeMode=%d CredSSPPreserved=%d Passed=%d\n", native, credssp, passed);
        fclose(output); return passed ? 0 : 1;
    }
}
