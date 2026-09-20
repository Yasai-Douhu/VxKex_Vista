#pragma once
// Shared by the shell configuration helper and VistaRun. Do not apply Code
// command-line switches to arbitrary executables with the same filename.
#include <windows.h>
#include <winver.h>
static BOOL VistaIsVSCode(PCWSTR path) {
    DWORD ignored, size = GetFileVersionInfoSizeW(path, &ignored);
    void *data; WCHAR *name; UINT length, translationsSize, i;
    struct TRANSLATION { WORD language, codepage; } *translations;
    WCHAR query[80]; BOOL match = FALSE;
    if (!size) return FALSE;
    data = HeapAlloc(GetProcessHeap(), 0, size);
    if (!data) return FALSE;
    if (GetFileVersionInfoW(path, 0, size, data) &&
        VerQueryValueW(data, L"\\VarFileInfo\\Translation", (void **)&translations, &translationsSize)) {
        for (i = 0; i < translationsSize / sizeof(*translations); ++i) {
            wsprintfW(query, L"\\StringFileInfo\\%04x%04x\\ProductName",
                translations[i].language, translations[i].codepage);
            if (VerQueryValueW(data, query, (void **)&name, &length) && length &&
                !lstrcmpW(name, L"Visual Studio Code")) { match = TRUE; break; }
        }
    }
    HeapFree(GetProcessHeap(), 0, data);
    return match;
}

// Delete only the exact launch command that we own. A replacement debugger
// belongs to its owner and must survive disabling/uninstalling VxKex.
static LONG VistaRemoveManagedDebugger(HKEY key) {
    WCHAR debugger[512], owner[512]; DWORD type, cb = sizeof(owner); LONG error;
    error = RegQueryValueExW(key, L"KEX_VistaDebugger", NULL, &type, (BYTE *)owner, &cb);
    if (error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (error) return error;
    if (type != REG_SZ || cb < sizeof(WCHAR) || cb > sizeof(owner) || cb % 2 || owner[cb / 2 - 1])
        return ERROR_INVALID_DATA;
    cb = sizeof(debugger);
    error = RegQueryValueExW(key, L"Debugger", NULL, &type, (BYTE *)debugger, &cb);
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND && error != ERROR_MORE_DATA) return error;
    if (!error && type == REG_SZ && cb >= sizeof(WCHAR) && cb <= sizeof(debugger) &&
        !(cb % 2) && !debugger[cb / 2 - 1] && !lstrcmpW(debugger, owner)) {
        error = RegDeleteValueW(key, L"Debugger");
        if (error && error != ERROR_FILE_NOT_FOUND) return error;
    }
    error = RegDeleteValueW(key, L"KEX_VistaDebugger");
    return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
}
