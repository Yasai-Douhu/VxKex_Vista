#include "buildcfg.h"
#include "kxcomp.h"
#include <ShObjIdl.h>
#include <propsys.h>

static SRWLOCK ShellLinkLock = SRWLOCK_INIT;
static HRESULT (STDMETHODCALLTYPE *NativeShellLinkCommit)(IPropertyStore *);
static const IPropertyStoreVtbl *HookedShellLinkTable;
static const GUID AppUserModel = {0x9f4c2855,0x9f79,0x4b39,{0xa8,0xd0,0xe1,0xd4,0x2d,0xe1,0xd5,0xf3}};

static HRESULT STDMETHODCALLTYPE VistaShellLinkCommit(IPropertyStore *Store)
{
    HRESULT Result = NativeShellLinkCommit(Store);
    DWORD Count, Index;
    IShellLinkW *Link;
    if (Result != E_INVALIDARG || !KexData ||
        !(KexData->Flags & KEXDATA_FLAG_INNO_SETUP) ||
        KexData->IfeoParameters.DisableAppSpecific) return Result;
    // Never hide failures in other property stores sharing implementation code.
    if (FAILED(Store->lpVtbl->QueryInterface(Store, &IID_IShellLinkW, (void **)&Link))) return Result;
    Link->lpVtbl->Release(Link);
    if (FAILED(Store->lpVtbl->GetCount(Store, &Count)) || !Count || Count > 5) return Result;
    for (Index = 0; Index < Count; ++Index) {
        PROPERTYKEY Key;
        PROPVARIANT Value;
        BOOL Optional;
        if (FAILED(Store->lpVtbl->GetAt(Store, Index, &Key)) ||
            !IsEqualGUID(&Key.fmtid, &AppUserModel)) return Result;
        ZeroMemory(&Value, sizeof(Value));
        if (FAILED(Store->lpVtbl->GetValue(Store, &Key, &Value))) return Result;
        Optional = (Key.pid == 5 && (Value.vt == VT_BSTR || Value.vt == VT_LPWSTR)) ||
            ((Key.pid == 8 || Key.pid == 9) && Value.vt == VT_BOOL) ||
            (Key.pid == 12 && Value.vt == VT_UI4) || (Key.pid == 26 && Value.vt == VT_CLSID);
        PropVariantClear(&Value);
        if (!Optional) return Result;
    }
    // NT 6.0 accepts these Win7+ taskbar/Win10 toast properties in memory but
    // cannot commit them. Omit only this optional metadata. The caller must
    // still use native IPersistFile::Save, whose errors remain untouched.
    KexLogDebugEvent(L"Omitting unsupported Inno ShellLink AppUserModel metadata on NT 6.0");
    return S_OK;
}

VOID ComConfigureInnoShellLink(REFCLSID ClassId, IUnknown *Object)
{
    ULONG Major, Minor;
    IPropertyStore *Store;
    DWORD OldProtection, Ignored;
    PVOID *Slot;
    if (!Object || !IsEqualGUID(ClassId, &CLSID_ShellLink) || !KexData ||
        !(KexData->Flags & KEXDATA_FLAG_INNO_SETUP) || KexData->IfeoParameters.DisableAppSpecific) return;
    KexRtlGetNtVersionNumbers(&Major, &Minor, NULL);
    if (Major != 6 || Minor != 0) return;
    if (FAILED(Object->lpVtbl->QueryInterface(Object, &IID_IPropertyStore, (void **)&Store))) return;
    AcquireSRWLockExclusive(&ShellLinkLock);
    if (!HookedShellLinkTable) {
        Slot = (PVOID *)&Store->lpVtbl->Commit;
        if (VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &OldProtection)) {
            NativeShellLinkCommit = Store->lpVtbl->Commit;
            InterlockedExchangePointer(Slot, (PVOID)VistaShellLinkCommit);
            HookedShellLinkTable = Store->lpVtbl;
            VirtualProtect(Slot, sizeof(*Slot), OldProtection, &Ignored);
        }
    }
    ReleaseSRWLockExclusive(&ShellLinkLock);
    Store->lpVtbl->Release(Store);
}
