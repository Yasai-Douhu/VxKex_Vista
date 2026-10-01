#include "buildcfg.h"
#include <KxCfgHlp.h>
#include <VistaLaunch.h>
#include <sddl.h>

#define SAVED_VERSION 1
#define VALUE_CAPACITY 65536
#define VIEW_STORE_VERSION 1

static LONG ReadDword(HKEY Key, PCWSTR Name, PDWORD Value, DWORD Default)
{
    DWORD Type, Bytes = sizeof(*Value);
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Value, &Bytes);
    if (Error == ERROR_FILE_NOT_FOUND) { *Value = Default; return 0; }
    if (Error) return Error;
    return Type == REG_DWORD && Bytes == sizeof(*Value) ? 0 : ERROR_INVALID_DATA;
}
static LONG WriteDword(HKEY Key, PCWSTR Name, DWORD Value)
{
    return RegSetValueEx(Key, Name, 0, REG_DWORD, (PCBYTE)&Value, sizeof(Value));
}
static LONG ReadRequiredDword(HKEY Key, PCWSTR Name, PDWORD Value)
{
    DWORD Bytes = 0;
    LONG Error = RegQueryValueEx(Key, Name, NULL, NULL, NULL, &Bytes);
    if (Error == ERROR_FILE_NOT_FOUND) return ERROR_INVALID_DATA;
    if (Error) return Error;
    return ReadDword(Key, Name, Value, 0);
}
static LONG ValidateValue(DWORD Type, PCBYTE Data, DWORD Bytes)
{
    if (Type == REG_DWORD && Bytes != 4) return ERROR_INVALID_DATA;
    if (Type == REG_QWORD && Bytes != 8) return ERROR_INVALID_DATA;
    if ((Type == REG_SZ || Type == REG_EXPAND_SZ) &&
        (Bytes < 2 || (Bytes & 1) || ((PCWSTR)Data)[Bytes / 2 - 1])) return ERROR_INVALID_DATA;
    if (Type == REG_MULTI_SZ && (Bytes < 4 || (Bytes & 1) ||
        ((PCWSTR)Data)[Bytes / 2 - 1] || ((PCWSTR)Data)[Bytes / 2 - 2])) return ERROR_INVALID_DATA;
    return 0;
}
static LONG ReadText(HKEY Key, PCWSTR Name, PWSTR Text, DWORD Characters)
{
    DWORD Type, Bytes = Characters * sizeof(WCHAR);
    LONG Error = RegQueryValueEx(Key, Name, NULL, &Type, (PBYTE)Text, &Bytes);
    if (Error == ERROR_FILE_NOT_FOUND) { Text[0] = 0; return 0; }
    if (Error) return Error;
    return Type == REG_SZ && Bytes >= 2 && !(Bytes & 1) &&
        !Text[Bytes / 2 - 1] ? 0 : ERROR_INVALID_DATA;
}
static LONG WriteText(HKEY Key, PCWSTR Name, PCWSTR Text)
{
    return RegSetValueEx(Key, Name, 0, REG_SZ, (PCBYTE)Text,
        (DWORD)((wcslen(Text) + 1) * sizeof(WCHAR)));
}

// The caller opens both keys in the same transaction and must roll back on
// any error. The backup key must be empty and stays intact after restoration.
// NULL Backup disables owned settings without saving them (explicit removal).
// Keys, rather than paths, retain Vista's basename and WOW64 view identity.
LONG KxCfgpPreserveIfeoConfiguration(HKEY Source, HKEY Backup)
{
    DWORD Index = 0, Subkeys, Values, Type, Bytes, Characters, Enabled;
    DWORD GlobalFlag, VerifierFlags;
    BOOLEAN VerifierPresent;
    LONG Error;
    WCHAR Name[256], Verifiers[256], Owner[512], Debugger[512];
    PBYTE Data = NULL;
    if (Backup) {
        Error = RegQueryInfoKey(Backup, NULL, NULL, NULL, &Subkeys, NULL, NULL,
            &Values, NULL, NULL, NULL, NULL);
        if (Error) return Error;
        if (Subkeys || Values) return ERROR_ALREADY_EXISTS;
    }
    Error = ReadText(Source, L"VerifierDlls", Verifiers, ARRAYSIZE(Verifiers));
    if (Error) return Error;
    VerifierPresent = KxCfgpRemoveKexDllFromVerifierDlls(Verifiers);
    Error = ReadDword(Source, L"GlobalFlag", &GlobalFlag, 0);
    if (Error) return Error;
    Enabled = VerifierPresent && !!(GlobalFlag & FLG_APPLICATION_VERIFIER);
    Error = ReadDword(Source, L"VerifierFlags", &VerifierFlags, 0);
    if (Error) return Error;
    Error = ReadText(Source, L"KEX_VistaDebugger", Owner, ARRAYSIZE(Owner));
    if (Error) return Error;
    Error = ReadText(Source, L"Debugger", Debugger, ARRAYSIZE(Debugger));
    if (Error) return Error;
    if (Backup && Owner[0] && Debugger[0] && !wcscmp(Owner, Debugger)) {
        Error = WriteText(Backup, L"SavedDebugger", Debugger);
        if (Error) return Error;
    }
    Data = HeapAlloc(GetProcessHeap(), 0, VALUE_CAPACITY);
    if (!Data) return ERROR_NOT_ENOUGH_MEMORY;
    for (;;) {
        Characters = ARRAYSIZE(Name); Bytes = VALUE_CAPACITY;
        Error = RegEnumValue(Source, Index++, Name, &Characters, NULL, &Type, Data, &Bytes);
        if (Error == ERROR_NO_MORE_ITEMS) { Error = 0; break; }
        if (Error) goto Finished;
        if (!_wcsnicmp(Name, L"KEX_", 4)) {
            Error = ValidateValue(Type, Data, Bytes);
            if (Error) goto Finished;
            if (Backup) {
                Error = RegSetValueEx(Backup, Name, 0, Type, Data, Bytes);
                if (Error) goto Finished;
            }
        }
    }
    if (Backup) {
        Error = WriteDword(Backup, L"SavedEnabled", Enabled);
        if (Error) goto Finished;
        Error = WriteDword(Backup, L"SavedVerifierFlags", VerifierFlags);
        if (Error) goto Finished;
        Error = WriteDword(Backup, L"SavedVersion", SAVED_VERSION);
        if (Error) goto Finished;
    }
    Error = VistaRemoveManagedDebugger(Source);
    if (Error) goto Finished;
    for (Index = 0;;) {
        Characters = ARRAYSIZE(Name);
        Error = RegEnumValue(Backup ? Backup : Source, Index, Name, &Characters, NULL, NULL, NULL, NULL);
        if (Error == ERROR_NO_MORE_ITEMS) { Error = 0; break; }
        if (Error) goto Finished;
        if (!_wcsnicmp(Name, L"KEX_", 4)) {
            Error = RegDeleteValue(Source, Name);
            if (Error != 0 && Error != ERROR_FILE_NOT_FOUND) goto Finished;
        }
        if (Backup || _wcsnicmp(Name, L"KEX_", 4)) ++Index;
    }
    if (VerifierPresent) {
        if (Verifiers[0]) Error = WriteText(Source, L"VerifierDlls", Verifiers);
        else {
            Error = RegDeleteValue(Source, L"VerifierDlls");
            if (Error) goto Finished;
            Error = WriteDword(Source, L"GlobalFlag", GlobalFlag & ~FLG_APPLICATION_VERIFIER);
            if (Error) goto Finished;
            Error = RegDeleteValue(Source, L"VerifierFlags");
            if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
        }
    }
Finished:
    HeapFree(GetProcessHeap(), 0, Data);
    return Error;
}

LONG KxCfgpRestoreIfeoConfiguration(HKEY Source, HKEY Backup)
{
    DWORD Version, Enabled, SavedFlags, CurrentFlags, GlobalFlag;
    DWORD Index = 0, Characters, Bytes, Type, CurrentBytes, CurrentType;
    LONG Error;
    WCHAR Name[256], Verifiers[256], CheckVerifiers[256], Debugger[512], CurrentDebugger[512];
    PBYTE Data = NULL, Current = NULL;
    Error = ReadRequiredDword(Backup, L"SavedVersion", &Version);
    if (Error) return Error;
    if (Version != SAVED_VERSION) return ERROR_INVALID_DATA;
    Error = ReadRequiredDword(Backup, L"SavedEnabled", &Enabled);
    if (Error) return Error;
    if (Enabled > 1) return ERROR_INVALID_DATA;
    Error = ReadRequiredDword(Backup, L"SavedVerifierFlags", &SavedFlags);
    if (Error) return Error;
    Error = ReadText(Backup, L"SavedDebugger", Debugger, ARRAYSIZE(Debugger));
    if (Error) return Error;
    if (Debugger[0]) {
        // A saved command is restorable only with its matching ownership marker.
        Error = ReadText(Backup, L"KEX_VistaDebugger", CurrentDebugger, ARRAYSIZE(CurrentDebugger));
        if (Error) return Error;
        if (!CurrentDebugger[0] || wcscmp(Debugger, CurrentDebugger)) return ERROR_INVALID_DATA;
    }
    Error = ReadText(Source, L"Debugger", CurrentDebugger, ARRAYSIZE(CurrentDebugger));
    if (Error) return Error;
    if (Debugger[0] && CurrentDebugger[0] && wcscmp(Debugger, CurrentDebugger))
        return ERROR_ALREADY_EXISTS;
    Error = ReadText(Source, L"VerifierDlls", Verifiers, ARRAYSIZE(Verifiers));
    if (Error) return Error;
    Error = ReadDword(Source, L"GlobalFlag", &GlobalFlag, 0);
    if (Error) return Error;
    Error = ReadDword(Source, L"VerifierFlags", &CurrentFlags, 0);
    if (Error) return Error;
    StringCchCopy(CheckVerifiers, ARRAYSIZE(CheckVerifiers), Verifiers);
    KxCfgpRemoveKexDllFromVerifierDlls(CheckVerifiers);
    if (Enabled && CheckVerifiers[0] && CurrentFlags != SavedFlags) return ERROR_ALREADY_EXISTS;
    Data = HeapAlloc(GetProcessHeap(), 0, VALUE_CAPACITY);
    Current = HeapAlloc(GetProcessHeap(), 0, VALUE_CAPACITY);
    if (!Data || !Current) { Error = ERROR_NOT_ENOUGH_MEMORY; goto Finished; }
    for (;;) {
        Characters = ARRAYSIZE(Name); Bytes = VALUE_CAPACITY;
        Error = RegEnumValue(Backup, Index++, Name, &Characters, NULL, &Type, Data, &Bytes);
        if (Error == ERROR_NO_MORE_ITEMS) { Error = 0; break; }
        if (Error) goto Finished;
        if (_wcsnicmp(Name, L"KEX_", 4)) continue;
        Error = ValidateValue(Type, Data, Bytes);
        if (Error) goto Finished;
        CurrentBytes = VALUE_CAPACITY;
        Error = RegQueryValueEx(Source, Name, NULL, &CurrentType, Current, &CurrentBytes);
        if (!Error && (Type != CurrentType || Bytes != CurrentBytes || memcmp(Data, Current, Bytes))) {
            Error = ERROR_ALREADY_EXISTS; goto Finished;
        }
        if (Error && Error != ERROR_FILE_NOT_FOUND) goto Finished;
        Error = RegSetValueEx(Source, Name, 0, Type, Data, Bytes);
        if (Error) goto Finished;
    }
    if (Debugger[0]) {
        Error = WriteText(Source, L"Debugger", Debugger);
        if (Error) goto Finished;
    }
    if (Enabled) {
        StringCchCopy(CheckVerifiers, ARRAYSIZE(CheckVerifiers), Verifiers);
        if (!KxCfgpRemoveKexDllFromVerifierDlls(CheckVerifiers)) {
            SIZE_T Length = wcslen(Verifiers);
            HRESULT Result = StringCchCat(Verifiers, ARRAYSIZE(Verifiers),
                !Length || Verifiers[Length - 1] == L' ' || Verifiers[Length - 1] == L'\t' ?
                L"kexdll.dll" : L" kexdll.dll");
            if (FAILED(Result)) { Error = ERROR_INSUFFICIENT_BUFFER; goto Finished; }
        }
        Error = WriteText(Source, L"VerifierDlls", Verifiers);
        if (Error) goto Finished;
        Error = WriteDword(Source, L"GlobalFlag", GlobalFlag | FLG_APPLICATION_VERIFIER);
        if (Error) goto Finished;
        Error = WriteDword(Source, L"VerifierFlags", SavedFlags);
    }
Finished:
    if (Data) HeapFree(GetProcessHeap(), 0, Data);
    if (Current) HeapFree(GetProcessHeap(), 0, Current);
    return Error;
}

// A view store contains numbered records, not executable paths as key names.
// Relative IFEO key identity therefore survives removed files and basename-only
// settings. The caller owns one transaction spanning ALL views and the store.
static LONG HasConfiguration(HKEY Key, PBOOL Present)
{
    DWORD Index, Characters;
    LONG Error;
    WCHAR Name[256], Text[512];
    PWSTR *Arguments;
    int Count;
    *Present = FALSE;
    // Legacy loader configurations need migration, not silent omission during
    // uninstall. Even a key with KEX_ values can still use that old launcher.
    Error = ReadText(Key, L"Debugger", Text, ARRAYSIZE(Text));
    if (Error) return Error;
    if (Text[0]) {
        Arguments = CommandLineToArgvW(Text, &Count);
        if (!Arguments) return GetLastError();
        Error = Count > 0 && !_wcsicmp(PathFindFileName(Arguments[0]), L"VxKexLdr.exe") ?
            ERROR_NOT_SUPPORTED : 0;
        LocalFree(Arguments);
        if (Error) return Error;
    }
    for (Index = 0;; ++Index) {
        Characters = ARRAYSIZE(Name);
        Error = RegEnumValue(Key, Index, Name, &Characters, NULL, NULL, NULL, NULL);
        if (Error == ERROR_NO_MORE_ITEMS) break;
        if (Error) return Error;
        if (!_wcsnicmp(Name, L"KEX_", 4)) { *Present = TRUE; return 0; }
    }
    Error = ReadText(Key, L"VerifierDlls", Text, ARRAYSIZE(Text));
    if (Error) return Error;
    if (KxCfgpRemoveKexDllFromVerifierDlls(Text)) { *Present = TRUE; return 0; }
    return 0;
}

static LONG SaveRecord(HKEY Source, HKEY Backup, PCWSTR Path, PCWSTR FilterPath,
    DWORD UseFilter, PDWORD Count, HANDLE Transaction)
{
    WCHAR Name[16];
    HKEY Record = NULL, Configuration = NULL;
    LONG Error;
    if (*Count == MAXDWORD) return ERROR_ARITHMETIC_OVERFLOW;
    StringCchPrintf(Name, ARRAYSIZE(Name), L"%08lx", *Count);
    Error = RegCreateKeyTransacted(Backup, Name, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY,
        NULL, &Record, NULL, Transaction, NULL);
    if (Error) return Error;
    Error = WriteText(Record, L"IfeoRelativePath", Path);
    if (Error) goto Done;
    if (FilterPath) {
        Error = WriteText(Record, L"FilterFullPath", FilterPath);
        if (Error) goto Done;
        Error = WriteDword(Record, L"UseFilter", UseFilter);
        if (Error) goto Done;
    }
    Error = RegCreateKeyTransacted(Record, L"Configuration", 0, NULL, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &Configuration, NULL, Transaction, NULL);
    if (!Error) Error = KxCfgpPreserveIfeoConfiguration(Source, Configuration);
    if (!Error) ++*Count;
Done:
    if (Configuration) RegCloseKey(Configuration);
    RegCloseKey(Record);
    return Error;
}

LONG KxCfgpPreserveIfeoView(HKEY IfeoRoot, HKEY Backup, HANDLE Transaction, REGSAM View)
{
    DWORD Index, ChildIndex, Characters, Children, Values, Count = 0, UseFilter;
    LONG Error;
    BOOL Present;
    WCHAR Name[256], ChildName[256], Path[512], FilterPath[MAX_PATH];
    HKEY Image = NULL, Filter = NULL;
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    if (View != 0 && View != KEY_WOW64_64KEY && View != KEY_WOW64_32KEY) return ERROR_INVALID_PARAMETER;
    if (Backup) {
        Error = RegQueryInfoKey(Backup, NULL, NULL, NULL, &Children, NULL, NULL,
            &Values, NULL, NULL, NULL, NULL);
        if (Error) return Error;
        if (Children || Values) return ERROR_ALREADY_EXISTS;
    }
    for (Index = 0;; ++Index) {
        Characters = ARRAYSIZE(Name);
        Error = RegEnumKeyEx(IfeoRoot, Index, Name, &Characters, NULL, NULL, NULL, NULL);
        if (Error == ERROR_NO_MORE_ITEMS) { Error = 0; break; }
        if (Error) goto Done;
        if (Backup && !_wcsicmp(Name, L"{VxKexPropagationVirtualKey}")) continue;
        Error = RegOpenKeyTransacted(IfeoRoot, Name, 0, KEY_READ | KEY_SET_VALUE | View,
            &Image, Transaction, NULL);
        if (Error) goto Done;
        Error = HasConfiguration(Image, &Present);
        if (Error) goto Done;
        if (Present) {
            Error = Backup ? SaveRecord(Image, Backup, Name, NULL, 0, &Count, Transaction) :
                KxCfgpPreserveIfeoConfiguration(Image, NULL);
            if (Error) goto Done;
        }
        Error = ReadDword(Image, L"UseFilter", &UseFilter, 0);
        if (Error) goto Done;
        if (UseFilter) {
            for (ChildIndex = 0;; ++ChildIndex) {
                Characters = ARRAYSIZE(ChildName);
                Error = RegEnumKeyEx(Image, ChildIndex, ChildName, &Characters,
                    NULL, NULL, NULL, NULL);
                if (Error == ERROR_NO_MORE_ITEMS) { Error = 0; break; }
                if (Error) goto Done;
                Error = RegOpenKeyTransacted(Image, ChildName, 0, KEY_READ | KEY_SET_VALUE | View,
                    &Filter, Transaction, NULL);
                if (Error) goto Done;
                Error = HasConfiguration(Filter, &Present);
                if (Error) goto Done;
                if (Present) {
                    Error = ReadText(Filter, L"FilterFullPath", FilterPath, ARRAYSIZE(FilterPath));
                    if (Error) goto Done;
                    if (!FilterPath[0] || PathIsRelative(FilterPath) ||
                        _wcsicmp(PathFindFileName(FilterPath), Name)) {
                        Error = ERROR_INVALID_DATA; goto Done;
                    }
                    if (FAILED(StringCchPrintf(Path, ARRAYSIZE(Path), L"%s\\%s", Name, ChildName))) {
                        Error = ERROR_INSUFFICIENT_BUFFER; goto Done;
                    }
                    Error = Backup ? SaveRecord(Filter, Backup, Path, FilterPath, UseFilter, &Count, Transaction) :
                        KxCfgpPreserveIfeoConfiguration(Filter, NULL);
                    if (Error) goto Done;
                }
                RegCloseKey(Filter); Filter = NULL;
            }
        }
        RegCloseKey(Image); Image = NULL;
    }
    if (Backup) {
        Error = WriteDword(Backup, L"RecordCount", Count);
        if (!Error) Error = WriteDword(Backup, L"IfeoView", View);
        if (!Error) Error = WriteDword(Backup, L"StoreVersion", VIEW_STORE_VERSION);
    }
Done:
    if (Filter) RegCloseKey(Filter);
    if (Image) RegCloseKey(Image);
    return Error;
}

static BOOLEAN ValidSegment(PCWSTR Segment)
{
    return Segment[0] && wcscmp(Segment, L".") && wcscmp(Segment, L"..") &&
        !wcschr(Segment, L'\\') && !wcschr(Segment, L'/') &&
        _wcsicmp(Segment, L"{VxKexPropagationVirtualKey}");
}

LONG KxCfgpRestoreIfeoView(HKEY IfeoRoot, HKEY Backup, HANDLE Transaction, REGSAM View)
{
    DWORD Version, Count, Index, Children, Values, UseFilter, CurrentUseFilter, Bytes;
    LONG Error;
    WCHAR Name[16], Path[512], FilterPath[MAX_PATH], CurrentPath[MAX_PATH];
    PWSTR Separator;
    HKEY Record = NULL, Configuration = NULL, Image = NULL, Target = NULL;
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    if (View != 0 && View != KEY_WOW64_64KEY && View != KEY_WOW64_32KEY) return ERROR_INVALID_PARAMETER;
    Error = ReadRequiredDword(Backup, L"StoreVersion", &Version);
    if (Error) return Error;
    if (Version != VIEW_STORE_VERSION) return ERROR_INVALID_DATA;
    Error = ReadRequiredDword(Backup, L"IfeoView", &Version);
    if (Error) return Error;
    if (Version != View) return ERROR_INVALID_DATA;
    Error = ReadRequiredDword(Backup, L"RecordCount", &Count);
    if (Error) return Error;
    Error = RegQueryInfoKey(Backup, NULL, NULL, NULL, &Children, NULL, NULL,
        &Values, NULL, NULL, NULL, NULL);
    if (Error) return Error;
    if (Children != Count || Values != 3) return ERROR_INVALID_DATA;
    for (Index = 0; Index < Count; ++Index) {
        StringCchPrintf(Name, ARRAYSIZE(Name), L"%08lx", Index);
        Error = RegOpenKeyTransacted(Backup, Name, 0, KEY_READ | KEY_WOW64_64KEY, &Record, Transaction, NULL);
        if (Error) goto Done;
        Error = RegQueryInfoKey(Record, NULL, NULL, NULL, &Children, NULL, NULL,
            &Values, NULL, NULL, NULL, NULL);
        if (Error) goto Done;
        Error = ReadText(Record, L"IfeoRelativePath", Path, ARRAYSIZE(Path));
        if (Error) goto Done;
        Separator = wcschr(Path, L'\\');
        if (Separator) *Separator++ = 0;
        if (!ValidSegment(Path) || (Separator && !ValidSegment(Separator)) ||
            Children != 1 || Values != (Separator ? 3u : 1u)) {
            Error = ERROR_INVALID_DATA; goto Done;
        }
        Error = RegOpenKeyTransacted(Record, L"Configuration", 0, KEY_READ | KEY_WOW64_64KEY,
            &Configuration, Transaction, NULL);
        if (Error) goto Done;
        Error = RegCreateKeyTransacted(IfeoRoot, Path, 0, NULL, 0, KEY_ALL_ACCESS | View,
            NULL, &Image, NULL, Transaction, NULL);
        if (Error) goto Done;
        if (Separator) {
            Error = ReadText(Record, L"FilterFullPath", FilterPath, ARRAYSIZE(FilterPath));
            if (Error) goto Done;
            if (!FilterPath[0] || PathIsRelative(FilterPath) ||
                _wcsicmp(PathFindFileName(FilterPath), Path)) {
                Error = ERROR_INVALID_DATA; goto Done;
            }
            Error = ReadRequiredDword(Record, L"UseFilter", &UseFilter);
            if (Error) goto Done;
            if (!UseFilter) { Error = ERROR_INVALID_DATA; goto Done; }
            Bytes = 0;
            Error = RegQueryValueEx(Image, L"UseFilter", NULL, NULL, NULL, &Bytes);
            if (!Error) {
                Error = ReadRequiredDword(Image, L"UseFilter", &CurrentUseFilter);
                if (Error) goto Done;
                if (CurrentUseFilter != UseFilter) { Error = ERROR_ALREADY_EXISTS; goto Done; }
            } else if (Error != ERROR_FILE_NOT_FOUND) goto Done;
            Error = RegCreateKeyTransacted(Image, Separator, 0, NULL, 0, KEY_ALL_ACCESS | View,
                NULL, &Target, NULL, Transaction, NULL);
            if (Error) goto Done;
            Error = ReadText(Target, L"FilterFullPath", CurrentPath, ARRAYSIZE(CurrentPath));
            if (Error) goto Done;
            if (CurrentPath[0] && _wcsicmp(CurrentPath, FilterPath)) {
                Error = ERROR_ALREADY_EXISTS; goto Done;
            }
            Error = WriteText(Target, L"FilterFullPath", FilterPath);
            if (Error) goto Done;
            Error = WriteDword(Image, L"UseFilter", UseFilter);
            if (Error) goto Done;
        }
        Error = KxCfgpRestoreIfeoConfiguration(Target ? Target : Image, Configuration);
        if (Error) goto Done;
        if (Target) { RegCloseKey(Target); Target = NULL; }
        RegCloseKey(Image); Image = NULL;
        RegCloseKey(Configuration); Configuration = NULL;
        RegCloseKey(Record); Record = NULL;
    }
    Error = 0;
Done:
    if (Target) RegCloseKey(Target);
    if (Image) RegCloseKey(Image);
    if (Configuration) RegCloseKey(Configuration);
    if (Record) RegCloseKey(Record);
    return Error;
}

#define CONFIGURATION_STORE L"Software\\VXsoft\\VxKexVistaPreserved"
#define IFEO_ROOT L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"

LONG KxCfgpProcessConfigurationRoots(BOOLEAN Restore, HANDLE Transaction,
    HKEY StoreParent, PCWSTR StorePath, HKEY NativeIfeo, HKEY WowIfeo)
{
    RTL_OSVERSIONINFOEXW Version = {sizeof(Version)};
    DWORD Bits = RtlOperatingSystemBitness(), ViewCount, StoredVersion, StoredBits;
    DWORD StoredCount, Children, Values, Disposition, Index;
    REGSAM Views[2];
    HKEY Store = NULL, ViewStore = NULL, Ifeo = NULL;
    PSECURITY_DESCRIPTOR Descriptor = NULL;
    SECURITY_ATTRIBUTES Security = {sizeof(Security), NULL, FALSE};
    LONG Error;
    NTSTATUS Status;
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE || !StoreParent || !StorePath || !NativeIfeo || !WowIfeo) return ERROR_INVALID_PARAMETER;
    Status = RtlGetVersion(&Version);
    if (!NT_SUCCESS(Status)) return RtlNtStatusToDosError(Status);
    ViewCount = Bits == 64 && Version.dwMajorVersion == 6 && Version.dwMinorVersion == 0 ? 2 : 1;
    Views[0] = Bits == 64 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;
    Views[1] = KEY_WOW64_32KEY;
    if (Restore) {
        Error = RegOpenKeyTransacted(StoreParent, StorePath, 0,
            KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Store, Transaction, NULL);
        if (Error == ERROR_FILE_NOT_FOUND) return 0;
        if (Error) return Error;
        Error = ReadRequiredDword(Store, L"StoreVersion", &StoredVersion);
        if (Error) goto Done;
        Error = ReadRequiredDword(Store, L"OperatingSystemBits", &StoredBits);
        if (Error) goto Done;
        Error = ReadRequiredDword(Store, L"ViewCount", &StoredCount);
        if (Error) goto Done;
        Error = RegQueryInfoKey(Store, NULL, NULL, NULL, &Children, NULL, NULL,
            &Values, NULL, NULL, NULL, NULL);
        if (Error) goto Done;
        if (StoredVersion != 1 || StoredBits != Bits || StoredCount != ViewCount ||
            Children != ViewCount || Values != 3) { Error = ERROR_INVALID_DATA; goto Done; }
    } else {
        // A persisted command must never come from a store writable by users.
        if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
            L"D:P(A;CI;KA;;;SY)(A;CI;KA;;;BA)", SDDL_REVISION_1, &Descriptor, NULL))
            return GetLastError();
        Security.lpSecurityDescriptor = Descriptor;
        Error = RegCreateKeyTransacted(StoreParent, StorePath, 0,
            NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Security, &Store,
            &Disposition, Transaction, NULL);
        LocalFree(Descriptor);
        if (Error) return Error;
        if (Disposition != REG_CREATED_NEW_KEY) { Error = ERROR_ALREADY_EXISTS; goto Done; }
    }
    for (Index = 0; Index < ViewCount; ++Index) {
        PCWSTR Name = Views[Index] == KEY_WOW64_64KEY ? L"64" : L"32";
        if (Restore) Error = RegOpenKeyTransacted(Store, Name, 0,
            KEY_READ | KEY_WOW64_64KEY, &ViewStore, Transaction, NULL);
        else Error = RegCreateKeyTransacted(Store, Name, 0, NULL, 0,
            KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &ViewStore, NULL, Transaction, NULL);
        if (Error) goto Done;
        Error = RegOpenKeyTransacted(Views[Index] == KEY_WOW64_64KEY ? NativeIfeo : WowIfeo, L"", 0,
            KEY_ALL_ACCESS | Views[Index], &Ifeo, Transaction, NULL);
        if (Error) goto Done;
        Error = Restore ? KxCfgpRestoreIfeoView(Ifeo, ViewStore, Transaction, Views[Index]) :
            KxCfgpPreserveIfeoView(Ifeo, ViewStore, Transaction, Views[Index]);
        if (Error) goto Done;
        RegCloseKey(Ifeo); Ifeo = NULL;
        RegCloseKey(ViewStore); ViewStore = NULL;
    }
    if (Restore) {
        // Deletion is part of the same transaction as every restored profile.
        Error = RegDeleteTree(Store, NULL);
        if (!Error) {
            RegCloseKey(Store); Store = NULL;
            Error = RegDeleteKeyTransacted(StoreParent, StorePath,
                KEY_WOW64_64KEY, 0, Transaction, NULL);
        }
    } else {
        Error = WriteDword(Store, L"OperatingSystemBits", Bits);
        if (!Error) Error = WriteDword(Store, L"ViewCount", ViewCount);
        if (!Error) Error = WriteDword(Store, L"StoreVersion", 1);
    }
Done:
    if (Ifeo) RegCloseKey(Ifeo);
    if (ViewStore) RegCloseKey(ViewStore);
    if (Store) RegCloseKey(Store);
    return Error;
}

static LONG ProcessAllConfigurations(BOOLEAN Restore, HANDLE Transaction)
{
    HKEY Native = NULL, Wow = NULL; LONG Error;
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE) return ERROR_INVALID_PARAMETER;
    Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO_ROOT, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Native, Transaction, NULL);
    if (!Error) Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO_ROOT, 0,
        KEY_ALL_ACCESS | KEY_WOW64_32KEY, &Wow, Transaction, NULL);
    if (!Error) Error = KxCfgpProcessConfigurationRoots(Restore, Transaction,
        HKEY_LOCAL_MACHINE, CONFIGURATION_STORE, Native, Wow);
    if (Native) RegCloseKey(Native);
    if (Wow) RegCloseKey(Wow);
    return Error;
}
KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgPreserveAllConfigurations(HANDLE Transaction)
{
    LONG Error = ProcessAllConfigurations(FALSE, Transaction);
    SetLastError(Error);
    return Error == 0;
}

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgRestoreAllConfigurations(HANDLE Transaction)
{
    LONG Error = ProcessAllConfigurations(TRUE, Transaction);
    SetLastError(Error);
    return Error == 0;
}

KXCFGDECLSPEC BOOLEAN KXCFGAPI KxCfgPrepareUninstall(BOOLEAN KeepSettings, HANDLE Transaction)
{
    RTL_OSVERSIONINFOEXW Version = {sizeof(Version)};
    DWORD Bits = RtlOperatingSystemBitness(), ViewCount, Index, StoreVersion;
    REGSAM View;
    HKEY Ifeo = NULL, Template = NULL, Store = NULL;
    LONG Error;
    NTSTATUS Status;
    if (!Transaction || Transaction == INVALID_HANDLE_VALUE) { Error = ERROR_INVALID_PARAMETER; goto Done; }
    Status = RtlGetVersion(&Version);
    if (!NT_SUCCESS(Status)) { Error = RtlNtStatusToDosError(Status); goto Done; }
    if (KeepSettings) {
        Error = ProcessAllConfigurations(FALSE, Transaction);
        if (Error) goto Done;
    }
    ViewCount = Bits == 64 && Version.dwMajorVersion == 6 && Version.dwMinorVersion == 0 ? 2 : 1;
    for (Index = 0; Index < ViewCount; ++Index) {
        View = Bits == 64 && Index == 0 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;
        Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, IFEO_ROOT, 0,
            KEY_ALL_ACCESS | View, &Ifeo, Transaction, NULL);
        if (Error) goto Done;
        if (KeepSettings) {
            // Do not persist the process-propagation template as a user profile.
            // Disable its provider while keeping any unrelated values or DLLs.
            Error = RegOpenKeyTransacted(Ifeo, L"{VxKexPropagationVirtualKey}", 0,
                KEY_ALL_ACCESS | View, &Template, Transaction, NULL);
            if (Error == ERROR_FILE_NOT_FOUND) Error = 0;
            else if (!Error) Error = KxCfgpPreserveIfeoConfiguration(Template, NULL);
            if (Template) { RegCloseKey(Template); Template = NULL; }
        } else Error = KxCfgpPreserveIfeoView(Ifeo, NULL, Transaction, View);
        if (Error) goto Done;
        RegCloseKey(Ifeo); Ifeo = NULL;
    }
    if (!KeepSettings) {
        // Explicit removal also discards a previous, owned preservation store.
        Error = RegOpenKeyTransacted(HKEY_LOCAL_MACHINE, CONFIGURATION_STORE, 0,
            KEY_ALL_ACCESS | KEY_WOW64_64KEY, &Store, Transaction, NULL);
        if (Error == ERROR_FILE_NOT_FOUND) { Error = 0; goto Done; }
        if (Error) goto Done;
        Error = ReadRequiredDword(Store, L"StoreVersion", &StoreVersion);
        if (Error) goto Done;
        if (StoreVersion != 1) { Error = ERROR_INVALID_DATA; goto Done; }
        Error = RegDeleteTree(Store, NULL);
        if (Error) goto Done;
        RegCloseKey(Store); Store = NULL;
        Error = RegDeleteKeyTransacted(HKEY_LOCAL_MACHINE, CONFIGURATION_STORE,
            KEY_WOW64_64KEY, 0, Transaction, NULL);
    }
Done:
    if (Template) RegCloseKey(Template);
    if (Ifeo) RegCloseKey(Ifeo);
    if (Store) RegCloseKey(Store);
    SetLastError(Error);
    return Error == 0;
}
