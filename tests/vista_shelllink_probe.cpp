// VS2010 /MT; each mode must run in a fresh process (DLL search policy is sticky).
// Usage: vista_shelllink_probe.exe <none|native|compat> <log> <output.lnk>
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

typedef BOOL (WINAPI *SETDEFAULT)(DWORD);
int wmain(int argc, wchar_t **argv)
{
    FILE *log;
    HMODULE module;
    SETDEFAULT setDefault;
    HRESULT hr;
    IShellLinkW *link = NULL;
    IPersistFile *persist = NULL;
    WCHAR target[MAX_PATH];
    if (argc != 4 || !(log = _wfopen(argv[2], L"w"))) return 2;
    fprintf(log, "ProcessBits=%u KexDllLoaded=%u\n", (unsigned)(sizeof(void*)*8),
        GetModuleHandleW(L"KexDll.dll") != NULL);
    {
        HMODULE kex = GetModuleHandleW(L"KexDll.dll");
        typedef LONG (WINAPI *GETDATA)(void**);
        GETDATA getData = kex ? (GETDATA)GetProcAddress(kex,"KexDataInitialize") : NULL;
        void *data = NULL;
        if (getData && getData(&data) >= 0 && data) fprintf(log,"KexFlags=%08lx\n",*(DWORD*)data);
    }
    if (wcscmp(argv[1], L"none")) {
        module = !wcscmp(argv[1], L"native") ? GetModuleHandleW(L"kernel32.dll") : LoadLibraryW(L"KxBase.dll");
        setDefault = module ? (SETDEFAULT)GetProcAddress(module,"SetDefaultDllDirectories") : NULL;
        if (!setDefault) { fprintf(log,"API unavailable: %lu\n",GetLastError()); fclose(log); return 3; }
        SetLastError(0);
        BOOL ok = setDefault(0x800); // LOAD_LIBRARY_SEARCH_SYSTEM32, as in Inno Setup.
        DWORD error = GetLastError();
        fprintf(log,"SetDefaultDllDirectories=%d error=%lu\n",ok,error);
        if (!ok) {
            // Inno's fallback removes CWD and preloads OS DLLs by absolute path.
            // ShellLink itself is resolved by COM; never replace it with a fake object.
            fprintf(log,"SetDllDirectory(empty)=%d\n",SetDllDirectoryW(L""));
        }
    }
    hr = CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    fprintf(log,"CoInitializeEx=%08lx\n",hr);
    if (FAILED(hr)) { fclose(log); return 4; }
    hr = CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void**)&link);
    fprintf(log,"CoCreateInstance=%08lx\n",hr);
    if (SUCCEEDED(hr)) {
        GetSystemDirectoryW(target,MAX_PATH);
        wcscat_s(target,MAX_PATH,L"\\notepad.exe");
        hr = link->SetPath(target);
        fprintf(log,"SetPath=%08lx\n",hr);
        if (SUCCEEDED(hr)) hr = link->QueryInterface(IID_IPersistFile,(void**)&persist);
        if (SUCCEEDED(hr)) {
            hr = persist->Save(argv[3],TRUE);
            fprintf(log,"Save=%08lx\n",hr);
            persist->Release();
        }
        link->Release();
    }
    CoUninitialize();
    fclose(log);
    return FAILED(hr) ? 1 : 0;
}
