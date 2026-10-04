#include "buildcfg.h"
#include "kexdllp.h"

typedef NTSTATUS (NTAPI *KEX_OPEN_KEY_EX)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,ULONG);

KEXAPI NTSTATUS NTAPI KexNtOpenKeyEx(
    PHANDLE KeyHandle,ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,ULONG OpenOptions)
{
    static PVOID volatile Cache;
    PVOID Address=InterlockedCompareExchangePointer(&Cache,NULL,NULL);
    PVOID Missing=(PVOID)(LONG_PTR)-1;
    if(!Address){
        PTEB Teb=NtCurrentTeb();ULONG Error=Teb->LastErrorValue;
        NTSTATUS LastStatus=Teb->LastStatusValue;
        try {Address=(PVOID)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtOpenKeyEx");}
        finally {Teb->LastErrorValue=Error;Teb->LastStatusValue=LastStatus;}
        if(!Address)Address=Missing;
        InterlockedCompareExchangePointer(&Cache,Address,NULL);
    }
    if(Address!=Missing)
        return ((KEX_OPEN_KEY_EX)Address)(KeyHandle,DesiredAccess,ObjectAttributes,OpenOptions);
    // Preserve native probing, alignment, ACL, relative roots and handle output.
    // Do not capture or modify OBJECT_ATTRIBUTES in user mode.
    // Native comparisons of owned registry links show that OBJ_OPENLINK in
    // ObjectAttributes selects the link itself with options 0 or 8 alike.
    // Preserve those attributes; do not synthesize OBJ_OPENLINK from option 8.
    if(!OpenOptions || OpenOptions==8 /* REG_OPTION_OPEN_LINK */)
        return NtOpenKey(KeyHandle,DesiredAccess,ObjectAttributes);
    // Extended options remain unsupported on NT 6.0. Never create a missing key
    // or silently drop backup/restore privileges or registry virtualization flags.
    return STATUS_NOT_SUPPORTED;
}
