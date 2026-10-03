#include "buildcfg.h"
#include "kexdllp.h"
#include <limits.h>

typedef struct _KEX_HANDLE_ENTRY32 {
    ULONG Object, ProcessId, HandleValue, GrantedAccess;
    USHORT TraceIndex, TypeIndex;
    ULONG Attributes, Reserved;
} KEX_HANDLE_ENTRY32;
typedef struct _KEX_HANDLE_ENTRY64 {
    ULONGLONG Object, ProcessId, HandleValue;
    ULONG GrantedAccess;
    USHORT TraceIndex, TypeIndex;
    ULONG Attributes, Reserved;
} KEX_HANDLE_ENTRY64;

#ifdef _M_IX86
typedef NTSTATUS (NTAPI *KEX_QUERY_PROCESS64)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef NTSTATUS (NTAPI *KEX_READ64)(HANDLE,ULONGLONG,PVOID,ULONGLONG,PULONGLONG);

// Four-argument x64 ABI bridge for the native NtQuerySystemInformation stub.
// Preserve x86 nonvolatile registers, align the native stack and reserve its
// shadow space. The return far pointer occupies exactly the eight bytes of
// the x64 call's return address; no stack DWORD is left behind.
__declspec(naked) static NTSTATUS __cdecl KexpQueryHandles64(
    ULONGLONG Address, ULONG Class, PVOID Buffer, ULONG Size, PULONG Returned)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov ebx, esp
        push 33h
        call enter64
    enter64:
        add dword ptr [esp], 5
        retf
        // x64: clear upper halves inherited from 32bit register writes.
        _emit 0x89
        _emit 0xED
        _emit 0x89
        _emit 0xDB
        // mov rax, [rbp+8]; mov ecx,[rbp+16]; mov edx,[rbp+20]
        _emit 0x48
        _emit 0x8B
        _emit 0x45
        _emit 0x08
        _emit 0x8B
        _emit 0x4D
        _emit 0x10
        _emit 0x8B
        _emit 0x55
        _emit 0x14
        // mov r8d,[rbp+24]; mov r9d,[rbp+28]
        _emit 0x44
        _emit 0x8B
        _emit 0x45
        _emit 0x18
        _emit 0x44
        _emit 0x8B
        _emit 0x4D
        _emit 0x1C
        // and rsp,-16; sub rsp,32; call rax; mov esp,ebx
        _emit 0x48
        _emit 0x83
        _emit 0xE4
        _emit 0xF0
        _emit 0x48
        _emit 0x83
        _emit 0xEC
        _emit 0x20
        _emit 0xFF
        _emit 0xD0
        _emit 0x89
        _emit 0xDC
        call leave64
    leave64:
        _emit 0xC7
        _emit 0x44
        _emit 0x24
        _emit 0x04
        _emit 0x23
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x83
        _emit 0x04
        _emit 0x24
        _emit 0x0D
        _emit 0xCB
        pop edi
        pop esi
        pop ebx
        mov esp, ebp
        pop ebp
        ret
    }
}

static ULONGLONG KexpFindNativeSystemQuery(VOID)
{
    HMODULE Module = GetModuleHandleW(L"ntdll.dll");
    KEX_QUERY_PROCESS64 Query = (KEX_QUERY_PROCESS64) GetProcAddress(Module, "NtWow64QueryInformationProcess64");
    KEX_READ64 Read = (KEX_READ64) GetProcAddress(Module, "NtWow64ReadVirtualMemory64");
    PROCESS_BASIC_INFORMATION64 Basic;
    ULONGLONG Ldr, Head, Entry, Base = 0;
    BYTE Record[104];
    WCHAR Name[32];
    ULONG Index;
    if (!Query || !Read || !NT_SUCCESS(Query(NtCurrentProcess(), ProcessBasicInformation, &Basic, sizeof(Basic), NULL))) return 0;
    if (!NT_SUCCESS(Read(NtCurrentProcess(), (ULONGLONG)Basic.PebBaseAddress + 0x18, &Ldr, sizeof(Ldr), NULL))) return 0;
    Head = Ldr + 0x10; // PEB_LDR_DATA64.InLoadOrderModuleList
    if (!NT_SUCCESS(Read(NtCurrentProcess(), Head, &Entry, sizeof(Entry), NULL))) return 0;
    for (Index = 0; Index < 128 && Entry != Head; ++Index) {
        USHORT Length;
        ULONGLONG NameAddress;
        if (!NT_SUCCESS(Read(NtCurrentProcess(), Entry, Record, sizeof(Record), NULL))) return 0;
        memcpy(&Length, Record + 0x58, sizeof(Length));
        memcpy(&NameAddress, Record + 0x60, sizeof(NameAddress));
        if (Length == 18 && NT_SUCCESS(Read(NtCurrentProcess(), NameAddress, Name, Length, NULL))) {
            ULONG Character;
            for (Character = 0; Character < 9; ++Character) {
                if ((Name[Character] | 0x20) != L"ntdll.dll"[Character]) break;
            }
            if (Character == 9) { memcpy(&Base, Record + 0x30, sizeof(Base)); break; }
        }
        memcpy(&Entry, Record, sizeof(Entry));
    }
    // NT 6.0 maps native ntdll below 4GB. Check this before using x86 pointers
    // to parse its PE64 export directory; never silently truncate an address.
    if (!Base || Base > ULONG_MAX) return 0;
    __try {
        PBYTE Image = (PBYTE)(ULONG_PTR)Base;
        IMAGE_DOS_HEADER *Dos = (IMAGE_DOS_HEADER*)Image;
        IMAGE_NT_HEADERS64 *Nt;
        IMAGE_EXPORT_DIRECTORY *Exports;
        ULONG *Names, *Functions;
        USHORT *Ordinals;
        if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0) return 0;
        Nt = (IMAGE_NT_HEADERS64*)(Image + Dos->e_lfanew);
        if (Nt->Signature != IMAGE_NT_SIGNATURE || Nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            Base + Nt->OptionalHeader.SizeOfImage > ULONG_MAX) return 0;
        Exports = (IMAGE_EXPORT_DIRECTORY*)(Image + Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
        Names = (ULONG*)(Image + Exports->AddressOfNames);
        Functions = (ULONG*)(Image + Exports->AddressOfFunctions);
        Ordinals = (USHORT*)(Image + Exports->AddressOfNameOrdinals);
        for (Index = 0; Index < Exports->NumberOfNames; ++Index) {
            if (!strcmp((PCHAR)(Image + Names[Index]),"NtQuerySystemInformation")) {
                ULONG Rva;
                if (Ordinals[Index] >= Exports->NumberOfFunctions) return 0;
                Rva = Functions[Ordinals[Index]];
                if (!Rva || Rva >= Nt->OptionalHeader.SizeOfImage) return 0;
                return Base + Rva;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 0;
}

static NTSTATUS KexpQueryNativeHandles(PVOID Buffer, ULONG Size, PULONG Returned)
{
    __declspec(align(8)) static LONGLONG Address;
    ULONGLONG Query = (ULONGLONG)InterlockedCompareExchange64(&Address, 0, 0);
    if (!Query) {
        Query = KexpFindNativeSystemQuery();
        if (!Query) return STATUS_NOT_SUPPORTED;
        InterlockedCompareExchange64(&Address, (LONGLONG)Query, 0);
    }
    return KexpQueryHandles64(Query, SystemExtendedHandleInformation, Buffer, Size, Returned);
}
#endif

static NTSTATUS KexpComparePinnedObjects(HANDLE First, HANDLE Second)
{
    HANDLE PinnedFirst = NULL, PinnedSecond = NULL;
    PVOID Buffer = NULL;
    BOOLEAN Native64;
    ULONG Size = 65536, Returned = 0, Attempt;
    ULONGLONG Count, Index, Object1 = 0, Object2 = 0;
    ULONG_PTR ProcessId = (ULONG_PTR)NtCurrentTeb()->ClientId.UniqueProcess;
    ULONG HeaderSize, EntrySize;
    NTSTATUS Status;
    Native64 = KexRtlOperatingSystemBitness() == 64;
    Status = NtDuplicateObject(NtCurrentProcess(), First, NtCurrentProcess(), &PinnedFirst, 0, 0, DUPLICATE_SAME_ACCESS);
    if (!NT_SUCCESS(Status)) return Status;
    if (First == Second) { NtClose(PinnedFirst); return STATUS_SUCCESS; }
    Status = NtDuplicateObject(NtCurrentProcess(), Second, NtCurrentProcess(), &PinnedSecond, 0, 0, DUPLICATE_SAME_ACCESS);
    if (!NT_SUCCESS(Status)) { NtClose(PinnedFirst); return Status; }
    HeaderSize = Native64 ? 16 : 8;
    EntrySize = Native64 ? sizeof(KEX_HANDLE_ENTRY64) : sizeof(KEX_HANDLE_ENTRY32);
    for (Attempt = 0; Attempt < 10; ++Attempt) {
        Buffer = RtlAllocateHeap(NtCurrentPeb()->ProcessHeap, 0, Size);
        if (!Buffer) { Status = STATUS_NO_MEMORY; break; }
#ifdef _M_IX86
        if (Native64) Status = KexpQueryNativeHandles(Buffer, Size, &Returned);
        else
#endif
        Status = NtQuerySystemInformation(SystemExtendedHandleInformation, Buffer, Size, &Returned);
        if (Status != STATUS_INFO_LENGTH_MISMATCH && Status != STATUS_BUFFER_TOO_SMALL) break;
        RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, Buffer); Buffer = NULL;
        if (Size >= 64 * 1024 * 1024 || Returned > 64 * 1024 * 1024) break;
        Size = max(Size * 2, Returned);
    }
    if (NT_SUCCESS(Status)) {
        Count = Native64 ? *(ULONGLONG*)Buffer : *(ULONG*)Buffer;
        if (Returned < HeaderSize || Returned > Size || Count > (Returned - HeaderSize) / EntrySize) {
            Status = STATUS_INFO_LENGTH_MISMATCH;
        } else {
            for (Index = 0; Index < Count; ++Index) {
                ULONGLONG Object, Pid, Handle;
                if (Native64) {
                    KEX_HANDLE_ENTRY64 *Entry = (KEX_HANDLE_ENTRY64*)((PBYTE)Buffer + HeaderSize) + (SIZE_T)Index;
                    Object = Entry->Object; Pid = Entry->ProcessId; Handle = Entry->HandleValue;
                } else {
                    KEX_HANDLE_ENTRY32 *Entry = (KEX_HANDLE_ENTRY32*)((PBYTE)Buffer + HeaderSize) + (SIZE_T)Index;
                    Object = Entry->Object; Pid = Entry->ProcessId; Handle = Entry->HandleValue;
                }
                if (Pid != ProcessId) continue;
                if (Handle == (ULONG_PTR)PinnedFirst) Object1 = Object;
                if (Handle == (ULONG_PTR)PinnedSecond) Object2 = Object;
                if (Object1 && Object2) break;
            }
            // Missing or suppressed identifiers cannot prove either identity or
            // inequality. Do not fall back to matching names or reference counts.
            Status = !Object1 || !Object2 ? STATUS_NOT_SUPPORTED :
                Object1 == Object2 ? STATUS_SUCCESS : STATUS_NOT_SAME_OBJECT;
        }
    }
    if (Buffer) RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, Buffer);
    NtClose(PinnedSecond); NtClose(PinnedFirst);
    return Status;
}

KEXAPI NTSTATUS NTAPI NtCompareObjects(HANDLE FirstObjectHandle, HANDLE SecondObjectHandle)
{
    typedef NTSTATUS (NTAPI *NATIVE_COMPARE)(HANDLE,HANDLE);
    static PVOID NativeCache;
    ULONG Error = GetLastError();
    NATIVE_COMPARE Native = (NATIVE_COMPARE)InterlockedCompareExchangePointer(&NativeCache, NULL, NULL);
    NTSTATUS Status;
    if (!Native) {
        Native = (NATIVE_COMPARE)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCompareObjects");
        if (!Native || Native == NtCompareObjects) Native = (NATIVE_COMPARE)1;
        InterlockedCompareExchangePointer(&NativeCache, (PVOID)Native, NULL);
    }
    Status = (ULONG_PTR)Native > 1 ? Native(FirstObjectHandle,SecondObjectHandle) :
        KexpComparePinnedObjects(FirstObjectHandle,SecondObjectHandle);
    SetLastError(Error);
    return Status;
}
