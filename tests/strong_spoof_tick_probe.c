#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>

int main(void)
{
    ULONGLONG start64, end64;
    DWORD start32, end32, error;
    unsigned i;
    int failed = 0;
    printf("KexDllLoaded=%d KxBaseLoaded=%d\n",
        GetModuleHandleW(L"KexDll.dll") != NULL,
        GetModuleHandleW(L"KxBase.dll") != NULL);
    start64 = GetTickCount64();
    start32 = GetTickCount();
    SetLastError(0x12345678);
    for (i = 0; i < 10000; ++i) {
        end64 = GetTickCount64();
        end32 = GetTickCount();
        if (end64 < start64 || (DWORD)(end32 - (DWORD)end64) > 100) failed = 1;
    }
    error = GetLastError();
    Sleep(250);
    end64 = GetTickCount64();
    end32 = GetTickCount();
    printf("Start64=%I64u End64=%I64u Delta64=%I64u Delta32=%lu LastError=%08lx\n",
        start64, end64, end64 - start64, end32 - start32, error);
    if (end64 - start64 < 100 || end64 - start64 > 10000 ||
        end32 - start32 < 100 || error != 0x12345678) failed = 1;
    printf("Result=%s\n", failed ? "FAIL" : "PASS");
    return failed;
}
