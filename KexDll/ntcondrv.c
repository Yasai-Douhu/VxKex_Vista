// NT 6.0 has a separate console handle namespace, not a ConDrv file device.
#include "buildcfg.h"
#include "kexdllp.h"
#undef NONLS
#undef _WINNLS_
#include <winnls.h>
#include <ZigNtIoProfile.h>
#include <limits.h>

static BOOL KexUseZigNtIo(VOID) {
    static LONG Profile; // 0 unknown, 1 selected, -1 not selected.
    LONG Selected;
    PBYTE Image;
    PIMAGE_NT_HEADERS Nt;
    if (!KexData || KexData->IfeoParameters.DisableAppSpecific ||
        OriginalMajorVersion != 6 || OriginalMinorVersion != 0) return FALSE;
    Selected = InterlockedCompareExchange(&Profile, 0, 0);
    if (!Selected) {
        Selected = -1;
        try {
            Image = (PBYTE)NtCurrentPeb()->ImageBaseAddress;
            Nt = RtlImageNtHeader(Image);
            if (Nt && ZigNtIoImage(Image, Nt->OptionalHeader.SizeOfImage)) Selected = 1;
        } except (EXCEPTION_EXECUTE_HANDLER) { Selected = -1; }
        InterlockedCompareExchange(&Profile, Selected, 0);
    }
    return Selected == 1;
}

// 0 native object, 1 console only, 2 ambiguous File/console numeric alias.
static ULONG KexConsoleWriteKind(HANDLE Handle) {
    typedef BOOL (WINAPI *VERIFY)(HANDLE);
    VERIFY Verify;
    NTSTATUS Status;
    union { ULONG_PTR Alignment; BYTE Bytes[512]; } Storage;
    UNICODE_STRING FileType;
    PRTL_USER_PROCESS_PARAMETERS Parameters;
    OBJECT_BASIC_INFORMATION Basic;
    if (!Handle || ((ULONG_PTR)Handle & 3) != 3 || GetFileType(Handle) != FILE_TYPE_CHAR) return 0;
    Verify = (VERIFY)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "VerifyConsoleIoHandle");
    if (!Verify || !Verify(Handle)) return 0;
    Status = NtQueryObject(Handle, ObjectTypeInformation, Storage.Bytes, sizeof(Storage.Bytes), NULL);
    if (Status == STATUS_INVALID_HANDLE) return 1;
    if (!NT_SUCCESS(Status)) return 2; // Never guess after an unexpected query failure.
    RtlInitConstantUnicodeString(&FileType, L"File");
    if (!RtlEqualUnicodeString(&((POBJECT_TYPE_INFORMATION)Storage.Bytes)->TypeName,
        &FileType, FALSE)) return 1;
    // Zig obtains its standard handles from ProcessParameters, then uses NT
    // I/O. NT 6.0 keeps console handles in a separate namespace: stdout 0x13
    // can alias the loader's directory File at 0x10. Recognize that specific
    // standard-console intent without treating arbitrary tagged files as
    // consoles. Never allow a writable native File alias through this path.
    Parameters=NtCurrentPeb()->ProcessParameters;
    if (!Parameters || (Handle != Parameters->StandardOutput &&
        Handle != Parameters->StandardError) ||
        (((ULONG_PTR)Parameters->CurrentDirectory.Handle & ~(ULONG_PTR)3) !=
         ((ULONG_PTR)Handle & ~(ULONG_PTR)3))) return 2;
    Status=NtQueryObject(Handle, ObjectBasicInformation, &Basic, sizeof(Basic), NULL);
    if (!NT_SUCCESS(Status) ||
        (Basic.GrantedAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA))) return 2;
    return 1;
}

static NTSTATUS KexConsoleWriteError(ULONG Error) {
    switch (Error) {
    case ERROR_INVALID_HANDLE: return STATUS_INVALID_HANDLE;
    case ERROR_ACCESS_DENIED: return STATUS_ACCESS_DENIED;
    case ERROR_NOACCESS: return STATUS_ACCESS_VIOLATION;
    case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: return STATUS_NO_MEMORY;
    case ERROR_NO_UNICODE_TRANSLATION: return STATUS_ILLEGAL_CHARACTER;
    case ERROR_OPERATION_ABORTED: return STATUS_CANCELLED;
    case ERROR_INVALID_PARAMETER: return STATUS_INVALID_PARAMETER;
    case ERROR_INVALID_FLAGS: return STATUS_NOT_SUPPORTED;
    default: return STATUS_UNSUCCESSFUL;
    }
}

// Full input was validated before I/O. A UTF-16 partial completion must be
// converted back to the consumed input BYTE count, never the wchar count.
static ULONG KexConsoleConsumed(UINT Cp, const BYTE *Bytes, ULONG Length, ULONG Written) {
    ULONG Offset=0, Characters=0, Width, Units;
    while (Offset < Length && Characters < Written) {
        Width=1; Units=1;
        if (Cp == CP_UTF8) {
            BYTE C=Bytes[Offset];
            if (C >= 0xf0) { Width=4; Units=2; }
            else if (C >= 0xe0) Width=3;
            else if (C >= 0xc0) Width=2;
        } else if (IsDBCSLeadByteEx(Cp, Bytes[Offset])) Width=2;
        if (Units > Written-Characters) break; // A half surrogate consumes no input character.
        Offset+=Width; Characters+=Units;
    }
    return Offset;
}

static BOOL KexWriteConsoleUnicode(HANDLE Handle, PCVOID Buffer, ULONG Count, PULONG Written) {
    typedef BOOL (WINAPI *CONSOLE_WRITE)(HANDLE,PCVOID,ULONG,PULONG,PVOID);
    // Reuse the existing VT implementation when that compatibility module is
    // already loaded. Do not create a new loader dependency for plain output.
    HMODULE Base=NULL;
    CONSOLE_WRITE Write;
    BOOL Result;
    GetModuleHandleExW(0, L"KxBase.dll", &Base); // A temporary reference prevents an unload race.
    Write=Base ? (CONSOLE_WRITE)GetProcAddress(Base,"WriteConsoleW") : NULL;
    try {
        Result=Write ? Write(Handle, Buffer, Count, Written, NULL) :
            WriteConsoleW(Handle, Buffer, Count, Written, NULL);
    } finally {
        ULONG Error=GetLastError();
        if (Base) FreeLibrary(Base);
        SetLastError(Error);
    }
    return Result;
}

KEXAPI NTSTATUS NTAPI Ext_NtWriteFile(HANDLE FileHandle, HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer, ULONG Length, PLONGLONG ByteOffset, PULONG Key)
{
    PTEB Teb=NtCurrentTeb(); ULONG OldError=Teb->LastErrorValue;
    NTSTATUS OldStatus=Teb->LastStatusValue, Status;
    ULONG Kind=0, I, Written=0, Consumed=0;
    BYTE *Captured=NULL; WCHAR *Unicode=NULL;
    UINT Cp; CPINFO CpInfo; INT Count;
    try {
        if (KexUseZigNtIo()) Kind=KexConsoleWriteKind(FileHandle);
    } finally { Teb->LastErrorValue=OldError; Teb->LastStatusValue=OldStatus; }
    if (!Kind) return NtWriteFile(FileHandle, Event, ApcRoutine, ApcContext,
        IoStatusBlock, Buffer, Length, ByteOffset, Key);
    if (Kind == 2 || Event || ApcRoutine || ApcContext || Key) return STATUS_NOT_SUPPORTED;
    try {
        try {
            // Probe the complete output before any console side effects. Memory
            // can still be unmapped by another thread, as for other user-mode APIs.
            for (I=0; I<sizeof(*IoStatusBlock); ++I) {
                volatile BYTE *P=(volatile BYTE *)IoStatusBlock+I;
                BYTE V=*P; *P=V;
            }
            if (ByteOffset && *ByteOffset != 0 && *ByteOffset != -1 && *ByteOffset != -2)
                return STATUS_INVALID_PARAMETER;
            if (Length) {
                if (Length > INT_MAX) return STATUS_INVALID_PARAMETER;
                Captured=(BYTE *)HeapAlloc(GetProcessHeap(), 0, Length);
                if (!Captured) return STATUS_NO_MEMORY;
                RtlCopyMemory(Captured, Buffer, Length);
                Cp=GetConsoleOutputCP();
                if (!GetCPInfo(Cp, &CpInfo)) return KexConsoleWriteError(GetLastError());
                if (Cp != CP_UTF8 && (Cp == CP_UTF7 || CpInfo.MaxCharSize > 2)) return STATUS_NOT_SUPPORTED;
                Count=MultiByteToWideChar(Cp, MB_ERR_INVALID_CHARS, (LPCSTR)Captured, Length, NULL, 0);
                if (!Count) return KexConsoleWriteError(GetLastError());
                if ((SIZE_T)Count > ((SIZE_T)-1)/sizeof(WCHAR)) return STATUS_NO_MEMORY;
                Unicode=(WCHAR *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)Count*sizeof(WCHAR));
                if (!Unicode) return STATUS_NO_MEMORY;
                if (!MultiByteToWideChar(Cp, MB_ERR_INVALID_CHARS, (LPCSTR)Captured, Length, Unicode, Count))
                    return KexConsoleWriteError(GetLastError());
                if (!KexWriteConsoleUnicode(FileHandle, Unicode, Count, &Written))
                    return KexConsoleWriteError(GetLastError());
                if (Written > (ULONG)Count) return STATUS_UNSUCCESSFUL;
                Consumed = Written == (ULONG)Count ? Length : KexConsoleConsumed(Cp, Captured, Length, Written);
                if (Cp == CP_UTF8 && Written && Written < (ULONG)Count &&
                    Unicode[Written-1] >= 0xd800 && Unicode[Written-1] <= 0xdbff &&
                    Unicode[Written] >= 0xdc00 && Unicode[Written] <= 0xdfff) {
                    // A split surrogate has already affected the console. Do
                    // not claim a successful byte completion or retry it blindly.
                    IoStatusBlock->Status=STATUS_ILLEGAL_CHARACTER;
                    IoStatusBlock->Information=Consumed;
                    return STATUS_ILLEGAL_CHARACTER;
                }
            }
            IoStatusBlock->Status=STATUS_SUCCESS; IoStatusBlock->Information=Consumed;
            Status=STATUS_SUCCESS;
        } except (EXCEPTION_EXECUTE_HANDLER) { Status=GetExceptionCode(); }
    } finally {
        if (Unicode) HeapFree(GetProcessHeap(), 0, Unicode);
        if (Captured) HeapFree(GetProcessHeap(), 0, Captured);
        Teb->LastErrorValue=OldError; Teb->LastStatusValue=OldStatus;
    }
    return Status;
}
