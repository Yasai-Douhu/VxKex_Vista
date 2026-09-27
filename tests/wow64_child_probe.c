#define _WIN32_WINNT 0x0600
#include <windows.h>
void mainCRTStartup(void) {
    static WCHAR Path[MAX_PATH];
    static STARTUPINFOW Startup = {sizeof(Startup)};
    PROCESS_INFORMATION Process;
    DWORD Length, Exit = 1, Written;
    typedef LONG (WINAPI *QUERY)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    QUERY Query;
    LONG (WINAPI *PropagationStatus)(void);
    PVOID (WINAPI *NativeBase)(void);
    ULONG Basic[6] = {0}, Wow = 0, Parameters = 0, Flags = 0;
    LONG BasicStatus, WowStatus;
    BYTE Hook[6] = {0};
    char Text[128];
    wsprintfA(Text, "ParentKexDllLoaded=%u\r\n", GetModuleHandleW(L"KexDll.dll") != NULL);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Text, lstrlenA(Text), &Written, NULL);
    PropagationStatus = (void *)GetProcAddress(GetModuleHandleW(L"KexDll.dll"), "KexGetPropagationStatus");
    NativeBase = (void *)GetProcAddress(GetModuleHandleW(L"KexDll.dll"), "KexLdrGetNativeSystemDllBase");
    wsprintfA(Text, "PropagationInitStatus=%08lx NativeNtdll=%08lx\r\n",
        PropagationStatus ? PropagationStatus() : 0xffffffff, NativeBase ? (ULONG)NativeBase() : 0);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Text, lstrlenA(Text), &Written, NULL);
    ReadProcessMemory(GetCurrentProcess(), GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateUserProcess"), Hook, 6, NULL);
    wsprintfA(Text, "ParentNtCreateUserProcess=%02x %02x %02x %02x %02x %02x\r\n",
        Hook[0], Hook[1], Hook[2], Hook[3], Hook[4], Hook[5]);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Text, lstrlenA(Text), &Written, NULL);
    Length = GetModuleFileNameW(NULL, Path, MAX_PATH);
    if (!Length || Length >= MAX_PATH) ExitProcess(2);
    while (Length && Path[Length-1] != L'\\') --Length;
    if (!Length || Length + 32 >= MAX_PATH) ExitProcess(2);
    lstrcpyW(Path + Length, L"VxKexChildNoIfeo.exe");
    Startup.dwFlags = STARTF_USESTDHANDLES;
    Startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    Startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    Startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessW(Path, NULL, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &Startup, &Process)) {
        wsprintfA(Text, "CreateProcessError=%lu\r\n", GetLastError());
        WriteFile(Startup.hStdOutput, Text, lstrlenA(Text), &Written, NULL);
        ExitProcess(2);
    }
    Query = (QUERY)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    if (Query) {
        BasicStatus = Query(Process.hProcess, 0, Basic, sizeof(Basic), NULL);
        WowStatus = Query(Process.hProcess, 26, &Wow, sizeof(Wow), NULL);
        wsprintfA(Text, "BasicStatus=%08lx Peb32=%08lx WowStatus=%08lx WowValue=%08lx\r\n",
            BasicStatus, Basic[1], WowStatus, Wow);
        WriteFile(Startup.hStdOutput, Text, lstrlenA(Text), &Written, NULL);
        if (BasicStatus >= 0 && ReadProcessMemory(Process.hProcess, (PVOID)(Basic[1]+0x10), &Parameters, 4, NULL) &&
            ReadProcessMemory(Process.hProcess, (PVOID)(Parameters+8), &Flags, 4, NULL)) {
            wsprintfA(Text, "ChildProcessParameterFlags=%08lx\r\n", Flags);
            WriteFile(Startup.hStdOutput, Text, lstrlenA(Text), &Written, NULL);
        }
    }
    if (ReadProcessMemory(Process.hProcess, GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtOpenKey"), Hook, 6, NULL)) {
        wsprintfA(Text, "ChildNtOpenKey=%02x %02x %02x %02x %02x %02x\r\n",
            Hook[0], Hook[1], Hook[2], Hook[3], Hook[4], Hook[5]);
        WriteFile(Startup.hStdOutput, Text, lstrlenA(Text), &Written, NULL);
    }
    ResumeThread(Process.hThread);
    WaitForSingleObject(Process.hProcess, INFINITE);
    GetExitCodeProcess(Process.hProcess, &Exit);
    wsprintfA(Text, "ChildExitCode=0x%08lx\r\n", Exit);
    WriteFile(Startup.hStdOutput, Text, lstrlenA(Text), &Written, NULL);
    CloseHandle(Process.hThread);
    CloseHandle(Process.hProcess);
    ExitProcess(Exit);
}
