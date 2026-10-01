#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <windows.h>
#include <stdio.h>
#include <sspi.h>

typedef PSecurityFunctionTableW (SEC_ENTRY *INIT_SECURITY_INTERFACE_W_FN)(void);

int main(void)
{
#if defined(_M_IX86)
    const wchar_t *dll = L"C:\\VxKexProbe\\NextParity\\KxSChanl32.dll";
    const char *log = "C:\\VxKexProbe\\NextParity\\load-result-x86.txt";
#else
    const wchar_t *dll = L"C:\\VxKexProbe\\NextParity\\KxSChanl.dll";
    const char *log = "C:\\VxKexProbe\\NextParity\\load-result-x64.txt";
#endif
    FILE *output = fopen(log, "w");
    HMODULE module;
    INIT_SECURITY_INTERFACE_W_FN initialize;
    PSecurityFunctionTableW table;
    ULONG packageCount = 0;
    PSecPkgInfoW packages = NULL;
    SECURITY_STATUS status;
    if (!output) return 2;
    setbuf(output, NULL);

    module = LoadLibraryW(dll);
    if (!module) {
        fprintf(output, "LoadLibraryW failed: %lu\n", GetLastError());
        fclose(output);
        return 1;
    }

    fprintf(output, "LoadLibraryW succeeded\n");
    fprintf(output, "InitSecurityInterfaceA=%p\n",
        GetProcAddress(module, "InitSecurityInterfaceA"));
    fprintf(output, "InitSecurityInterfaceW=%p\n",
        GetProcAddress(module, "InitSecurityInterfaceW"));
    initialize = (INIT_SECURITY_INTERFACE_W_FN)
        GetProcAddress(module, "InitSecurityInterfaceW");
    if (!initialize) {
        fclose(output);
        FreeLibrary(module);
        return 3;
    }
    table = initialize();
    fprintf(output, "SecurityFunctionTableW=%p\n", table);
    if (!table || !table->EnumerateSecurityPackagesW ||
        !table->FreeContextBuffer) {
        fclose(output);
        FreeLibrary(module);
        return 4;
    }
    status = table->EnumerateSecurityPackagesW(&packageCount, &packages);
    fprintf(output, "EnumerateSecurityPackagesW=0x%08lx count=%lu\n",
        (unsigned long)status, packageCount);
    if (status == SEC_E_OK && packages) {
        fprintf(output, "FirstPackagePresent=%d\n", packages[0].Name != NULL);
        table->FreeContextBuffer(packages);
        fprintf(output, "FreeContextBuffer completed\n");
    }
    // This SSP is intended to remain loaded for the lifetime of the process.
    // Explicit unload is a separate regression test; it currently crashes on x86.
    fclose(output);
    return status == SEC_E_OK && packageCount == 1 ? 0 : 5;
}
