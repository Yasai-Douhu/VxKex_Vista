// Try operations must never enter the native blocking acquisition path.
#include "buildcfg.h"
#include "kexdllp.h"

typedef BOOLEAN (NTAPI *KEX_SRW_TRY)(PRTL_SRWLOCK);
static KEX_SRW_TRY KexNativeSrwTry(PCSTR Name, PVOID volatile *Cache) {
    PVOID Address=InterlockedCompareExchangePointer(Cache,NULL,NULL);
    PVOID Missing=(PVOID)(LONG_PTR)-1;
    if (!Address) {
        PTEB Teb=NtCurrentTeb();ULONG Error=Teb->LastErrorValue;
        NTSTATUS LastStatus=Teb->LastStatusValue;
        try { Address=(PVOID)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),Name); }
        finally { Teb->LastErrorValue=Error;Teb->LastStatusValue=LastStatus; }
        if (!Address) Address=Missing;
        InterlockedCompareExchangePointer(Cache,Address,NULL);
    }
    return Address==Missing ? NULL : (KEX_SRW_TRY)Address;
}

static VOID KexRequireVistaSrwLayout(VOID) {
    if (OriginalMajorVersion!=6 || OriginalMinorVersion!=0)
        RaiseException(STATUS_NOT_SUPPORTED,EXCEPTION_NONCONTINUABLE,0,NULL);
}

KEXAPI BOOLEAN NTAPI KexRtlTryAcquireSRWLockExclusive(PRTL_SRWLOCK Lock) {
    static PVOID volatile Cache;
    KEX_SRW_TRY Native=KexNativeSrwTry("RtlTryAcquireSRWLockExclusive",&Cache);
    if (Native) return Native(Lock);
    KexRequireVistaSrwLayout();
    return InterlockedCompareExchangePointer((PVOID volatile *)&Lock->Ptr,
        (PVOID)1,NULL)==NULL;
}

KEXAPI BOOLEAN NTAPI KexRtlTryAcquireSRWLockShared(PRTL_SRWLOCK Lock) {
    static PVOID volatile Cache;
    KEX_SRW_TRY Native=KexNativeSrwTry("RtlTryAcquireSRWLockShared",&Cache);
    ULONG_PTR Old,Next;
    if (Native) return Native(Lock);
    KexRequireVistaSrwLayout();
    do {
        // Native try operations require writable storage even when busy.
        // The atomic read also preserves that fault behavior on readonly locks.
        Old=(ULONG_PTR)InterlockedCompareExchangePointer(
            (PVOID volatile *)&Lock->Ptr,NULL,NULL);
        if (Old & 0xe) return FALSE; // Native waiter pointer / transition flags.
        if (Old && (!(Old&1) || !(Old&~(ULONG_PTR)0xf))) return FALSE;
        if (Old > ~(ULONG_PTR)0 - 0x10) return FALSE; // Never wrap owner count.
        Next=(Old+0x10)|1;
    } while ((ULONG_PTR)InterlockedCompareExchangePointer(
        (PVOID volatile *)&Lock->Ptr,(PVOID)Next,(PVOID)Old)!=Old);
    return TRUE;
}
