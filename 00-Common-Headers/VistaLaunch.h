#pragma once
// Shared by the shell configuration helper and VistaRun. Do not apply Code
// command-line switches to arbitrary executables with the same filename.
#include <windows.h>
#include <winver.h>
// A newer PE subsystem requirement is checked before verifier injection.
// These fields have identical offsets in PE32 and PE32+ optional headers.
static BOOL VistaRequiresSubsystemLauncher(PCWSTR path) {
    HANDLE file;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    DWORD count;
    BOOL match = FALSE;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (ReadFile(file, &dos, sizeof(dos), &count, NULL) && count == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && dos.e_lfanew <= 0x1000 &&
        SetFilePointer(file, dos.e_lfanew, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(file, &nt, sizeof(nt), &count, NULL) && count == sizeof(nt)) {
        match = nt.Signature == IMAGE_NT_SIGNATURE &&
            ((nt.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
              nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) ||
             (nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
              nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)) &&
            nt.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER32) &&
            (nt.OptionalHeader.MajorSubsystemVersion > 6 ||
             (nt.OptionalHeader.MajorSubsystemVersion == 6 &&
              nt.OptionalHeader.MinorSubsystemVersion > 0));
    }
    CloseHandle(file);
    return match;
}
static BOOL VistaHasProductName(PCWSTR path, PCWSTR productName) {
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
                !lstrcmpW(name, productName)) { match = TRUE; break; }
        }
    }
    HeapFree(GetProcessHeap(), 0, data);
    return match;
}

static BOOL VistaIsVSCode(PCWSTR path) {
    return VistaHasProductName(path, L"Visual Studio Code");
}

static BOOL VistaUsesElectronLaunchProfile(PCWSTR path) {
    return VistaIsVSCode(path) || VistaHasProductName(path, L"Obsidian");
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
