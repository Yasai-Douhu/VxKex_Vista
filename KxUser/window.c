#include "buildcfg.h"
#include "kxuserp.h"

// Vista only has a process-wide UIPI filter. Preserve the native window
// filter on newer systems; on Vista allow/disallow use the legacy scope.
KXUSERAPI BOOL WINAPI Ext_ChangeWindowMessageFilterEx(
	HWND Window, UINT Message, DWORD Action, PCHANGEFILTERSTRUCT ChangeInfo)
{
	typedef BOOL (WINAPI *PNATIVE)(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT);
	PNATIVE Native = (PNATIVE) GetProcAddress(GetModuleHandleW(L"user32.dll"),
		"ChangeWindowMessageFilterEx");
	DWORD ProcessId = 0;
	BOOL Success;
	if (Native) return Native(Window, Message, Action, ChangeInfo);
	if (!GetWindowThreadProcessId(Window, &ProcessId)) {
		SetLastError(ERROR_INVALID_WINDOW_HANDLE);
		return FALSE;
	}
	if (ProcessId != GetCurrentProcessId()) {
		SetLastError(ERROR_ACCESS_DENIED);
		return FALSE;
	}
	if ((ChangeInfo && ChangeInfo->cbSize != sizeof(*ChangeInfo)) || Action > MSGFLT_DISALLOW) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	// Resetting one window cannot be represented by a process-wide operation.
	if (Action == MSGFLT_RESET) {
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}
	Success = ChangeWindowMessageFilter(Message,
		Action == MSGFLT_ALLOW ? MSGFLT_ADD : MSGFLT_REMOVE);
	if (Success && ChangeInfo) ChangeInfo->ExtStatus = MSGFLTINFO_NONE;
	return Success;
}

KXUSERAPI BOOL WINAPI IsWindowArranged(
	HWND	Window)
{
	return FALSE;
}
