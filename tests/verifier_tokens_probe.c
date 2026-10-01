#include "../KxCfgHlp/buildcfg.h"
#include <KxCfgHlp.h>
#include <stdio.h>
#ifdef _WIN64
#define LOGFILE "C:\\VxKexProbe\\NextParity\\verifier-tokens-x64.txt"
#else
#define LOGFILE "C:\\VxKexProbe\\NextParity\\verifier-tokens-x86.txt"
#endif
VOID __cdecl mainCRTStartup(VOID)
{
    struct { PCWSTR Input; PCWSTR Output; BOOLEAN Removed; } cases[] = {
        { L"", L"", FALSE },
        { L"kexdll.dll", L"", TRUE },
        { L"\t KEXDLL.DLL \t", L"", TRUE },
        { L"kexdll.dll kexdll.dll", L"", TRUE },
        { L"other.dll", L"other.dll", FALSE },
        { L"prefixkexdll.dll other.dll", L"prefixkexdll.dll other.dll", FALSE },
        { L"kexdll.dll.backup", L"kexdll.dll.backup", FALSE },
        { L"C:\\Other\\kexdll.dll", L"C:\\Other\\kexdll.dll", FALSE },
        { L"kexdll.dll Other.DLL", L" Other.DLL", TRUE },
        { L"Other.DLL\tkexdll.dll", L"Other.DLL\t", TRUE },
        { L"A.dll\tkexdll.dll  B.dll", L"A.dll\t  B.dll", TRUE },
        { L"KexDll.dll\tA.dll kexdll.dll B.dll", L"\tA.dll  B.dll", TRUE },
        { L"prefixkexdll.dll kexdll.dll kexdll.dll.backup", L"prefixkexdll.dll  kexdll.dll.backup", TRUE },
        { L" \t", L" \t", FALSE }
    };
    FILE *log = fopen(LOGFILE, "wt");
    ULONG index;
    int failures = 0;
    if (!log) ExitProcess(2);
    for (index = 0; index < ARRAYSIZE(cases); ++index) {
        WCHAR buffer[256];
        BOOLEAN removed;
        StringCchCopy(buffer, ARRAYSIZE(buffer), cases[index].Input);
        removed = KxCfgpRemoveKexDllFromVerifierDlls(buffer);
        fprintf(log, "Case[%lu] removed=%u textMatch=%u\n", index,
            removed, !wcscmp(buffer, cases[index].Output));
        if (removed != cases[index].Removed || wcscmp(buffer, cases[index].Output)) ++failures;
        if (removed && KxCfgpRemoveKexDllFromVerifierDlls(buffer)) ++failures;
    }
    fprintf(log, "Failures=%d\n", failures);
    fclose(log); ExitProcess(failures ? 1 : 0);
}
