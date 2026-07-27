#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include "buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>

int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	int nCmdShow)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	int i;
	
	KXCFG_PROGRAM_CONFIGURATION Config;
	WCHAR ExeFullPath[MAX_PATH] = {0};
	BOOLEAN ExeSet = FALSE;
	
	if (!argv) {
		return 1;
	}

	RtlZeroMemory(&Config, sizeof(Config));

	for (i = 1; i < argc; i++) {
		LPWSTR arg = argv[i];
		if (_wcsnicmp(arg, L"/EXE:", 5) == 0) {
			wcsncpy_s(ExeFullPath, MAX_PATH, arg + 5, _TRUNCATE);
			ExeSet = TRUE;
		} else if (_wcsnicmp(arg, L"/ENABLE:", 8) == 0) {
			Config.Enabled = wcstoul(arg + 8, NULL, 10);
		} else if (_wcsnicmp(arg, L"/DISABLEFORCHILD:", 17) == 0) {
			Config.DisableForChild = wcstoul(arg + 17, NULL, 10);
		} else if (_wcsnicmp(arg, L"/DISABLEAPPSPECIFIC:", 20) == 0) {
			Config.DisableAppSpecificHacks = wcstoul(arg + 20, NULL, 10);
		} else if (_wcsnicmp(arg, L"/WINVERSPOOF:", 13) == 0) {
			Config.WinVerSpoof = wcstoul(arg + 13, NULL, 10);
		} else if (_wcsnicmp(arg, L"/STRONGSPOOF:", 13) == 0) {
			Config.StrongSpoofOptions = wcstoul(arg + 13, NULL, 16);
		}
	}

	if (ExeSet) {
		KxCfgSetConfiguration(ExeFullPath, &Config, NULL);
	}

	LocalFree(argv);
	return 0;
}
