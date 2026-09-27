#define _WIN32_WINNT 0x0600
#include <windows.h>
static void print(const char *s) { DWORD n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&n,NULL); }
void mainCRTStartup(void) {
 static OSVERSIONINFOEXW Win = {sizeof(Win)}, Nt = {sizeof(Nt)};
 LONG (WINAPI *RtlVersion)(OSVERSIONINFOEXW *);
 HMODULE Kex = GetModuleHandleW(L"KexDll.dll");
 static WCHAR Module[MAX_PATH] = {0};
 char Text[512];
 BOOL Wow = FALSE;
 IsWow64Process(GetCurrentProcess(), &Wow);
 GetVersionExW((OSVERSIONINFOW *)&Win);
 RtlVersion = (void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion");
 if (RtlVersion) RtlVersion(&Nt);
 if (Kex) GetModuleFileNameW(Kex, Module, MAX_PATH);
 wsprintfA(Text,"ProcessBits=%u WOW64=%u KexDllLoaded=%u\r\nGetVersionEx=%lu.%lu.%lu\r\nRtlGetVersion=%lu.%lu.%lu\r\n",sizeof(void*)*8,Wow,Kex!=NULL,Win.dwMajorVersion,Win.dwMinorVersion,Win.dwBuildNumber,Nt.dwMajorVersion,Nt.dwMinorVersion,Nt.dwBuildNumber);
 print(Text);
 if (Kex) { wsprintfA(Text,"KexDllPath=%ls\r\n",Module); print(Text); }
 ExitProcess(Kex && Win.dwMajorVersion==10 && Nt.dwMajorVersion==10 ? 0 : 1);
}
