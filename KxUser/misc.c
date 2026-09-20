#include "buildcfg.h"
#include "kxuserp.h"

// Keep the explicit application ID within the process. Vista's taskbar has
// no AppUserModelID grouping support, but applications can set and read it.
STATIC SRWLOCK AppIdLock = SRWLOCK_INIT;
STATIC WCHAR AppId[129];
KXUSERAPI HRESULT WINAPI Ext_SetCurrentProcessExplicitAppUserModelID(PCWSTR Id)
{
	SIZE_T Length;
	if (!Id) return E_INVALIDARG;
	Length = wcslen(Id);
	if (!Length || Length > 128) return E_INVALIDARG;
	AcquireSRWLockExclusive(&AppIdLock);
	RtlCopyMemory(AppId, Id, (Length + 1) * sizeof(WCHAR));
	ReleaseSRWLockExclusive(&AppIdLock);
	return S_OK;
}
KXUSERAPI HRESULT WINAPI Ext_GetCurrentProcessExplicitAppUserModelID(PWSTR *Id)
{
	SIZE_T Bytes;
	if (!Id) return E_POINTER;
	*Id = NULL;
	AcquireSRWLockShared(&AppIdLock);
	if (!AppId[0]) {
		ReleaseSRWLockShared(&AppIdLock);
		return E_FAIL;
	}
	Bytes = (wcslen(AppId) + 1) * sizeof(WCHAR);
	*Id = (PWSTR) CoTaskMemAlloc(Bytes);
	if (*Id) RtlCopyMemory(*Id, AppId, Bytes);
	ReleaseSRWLockShared(&AppIdLock);
	return *Id ? S_OK : E_OUTOFMEMORY;
}

// Vista has no CCD query entry points. Let callers use their legacy
// EnumDisplayDevices/EnumDisplaySettings fallback rather than fail to load.
KXUSERAPI LONG WINAPI Ext_GetDisplayConfigBufferSizes(
	UINT32 Flags, UINT32 *Paths, UINT32 *Modes)
{
	if (!Paths || !Modes) return ERROR_INVALID_PARAMETER;
	return ERROR_NOT_SUPPORTED;
}
KXUSERAPI LONG WINAPI Ext_QueryDisplayConfig(
	UINT32 Flags, UINT32 *PathCount, PVOID Paths,
	UINT32 *ModeCount, PVOID Modes, PVOID Topology)
{
	return ERROR_NOT_SUPPORTED;
}
KXUSERAPI LONG WINAPI Ext_DisplayConfigGetDeviceInfo(PVOID Request)
{
	if (!Request) return ERROR_INVALID_PARAMETER;
	return ERROR_NOT_SUPPORTED;
}

KXUSERAPI BOOL WINAPI GetProcessUIContextInformation(
	IN	HANDLE							ProcessHandle,
	OUT	PPROCESS_UICONTEXT_INFORMATION	UIContextInformation)
{
	if (!UIContextInformation) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	UIContextInformation->UIContext = PROCESS_UICONTEXT_DESKTOP;
	UIContextInformation->Flags		= PROCESS_UIF_NONE;

	return TRUE;
}

KXUSERAPI HWND WINAPI CreateWindowInBand(
	IN	DWORD			dwExStyle,
	IN	PCWSTR			lpClassName,
	IN	PCWSTR			lpWindowName,
	IN	DWORD			dwStyle,
	IN	INT				X,
	IN	INT				Y,
	IN	INT				nWidth,
	IN	INT				nHeight,
	IN	HWND			hWndParent,
	IN	HMENU			hMenu,
	IN	HINSTANCE		hInstance,
	IN	PVOID			lpParam,
	IN	ZBID			zbid)
{
	return CreateWindowExW(
		dwExStyle,
		lpClassName,
		lpWindowName,
		dwStyle,
		X,
		Y,
		nWidth,
		nHeight,
		hWndParent,
		hMenu,
		hInstance,
		lpParam);
}

KXUSERAPI BOOL WINAPI GetWindowBand(
	IN	HWND	Window,
	OUT	PZBID	Band)
{
	if (!IsWindow(Window)) {
		SetLastError(ERROR_INVALID_WINDOW_HANDLE);
		return FALSE;
	}

	if (!Band) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (Window == GetDesktopWindow()) {
		*Band = ZBID_DESKTOP;
	} else {
		*Band = ZBID_DEFAULT;
	}

	return TRUE;
}

KXUSERAPI BOOL WINAPI GetCurrentInputMessageSource(
	OUT	PINPUT_MESSAGE_SOURCE	MessageSource)
{
	if (!MessageSource) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	MessageSource->DeviceType = IMDT_UNAVAILABLE;
	MessageSource->OriginId = IMO_UNAVAILABLE;
	return TRUE;
}

KXUSERAPI BOOL WINAPI IsImmersiveProcess(
	IN	HANDLE	ProcessHandle)
{
	SetLastError(ERROR_SUCCESS);
	return FALSE;
}
