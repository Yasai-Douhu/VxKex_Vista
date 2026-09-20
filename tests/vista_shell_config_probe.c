#include "../KxCfgHlp/buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <stdio.h>
static int failures;
static void check(BOOL result, const char *name) { printf("%s %s (error=%lu)\n", result ? "PASS" : "FAIL", name, GetLastError()); if (!result) ++failures; }
void mainCRTStartup(void) {
    KXCFG_PROGRAM_CONFIGURATION config = {0}; HKEY key; WCHAR value[512]; DWORD size, type; LONG error;
    const WCHAR path[] = L"C:\\VxKexProbe\\ShellConfigProbe.exe";
    const WCHAR reg[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\ShellConfigProbe.exe";
    config.Enabled = TRUE; config.WinVerSpoof = WinVerSpoofWin10; config.StrongSpoofOptions = 3;
    check(KxCfgSetConfiguration(path, &config, NULL), "enable");
    error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, reg, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key);
    check(!error, "open config"); if(error) ExitProcess(1);
    size = sizeof(value); error = RegQueryValueExW(key,L"Debugger",NULL,&type,(BYTE*)value,&size);
    check(!error && !lstrcmpW(value,L"\"C:\\VxKex\\VistaRun.exe\" --ifeo"), "registered launcher");
    config.Enabled = FALSE;
    check(KxCfgSetConfiguration(path, &config, NULL), "disable keeping preferences");
    size=sizeof(value); check(RegQueryValueExW(key,L"Debugger",NULL,&type,(BYTE*)value,&size)==ERROR_FILE_NOT_FOUND,"launcher removed");
    config.Enabled=TRUE; check(KxCfgSetConfiguration(path,&config,NULL),"reenable");
    check(KxCfgDeleteConfiguration(path,NULL),"delete configuration"); RegCloseKey(key);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE,reg,0,NULL,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,NULL,&key,NULL);
    RegSetValueExW(key,L"Debugger",0,REG_SZ,(BYTE*)L"foreign-debugger.exe",sizeof(L"foreign-debugger.exe"));
    check(!KxCfgSetConfiguration(path,&config,NULL) && GetLastError()==ERROR_ALREADY_EXISTS,"foreign debugger rejected");
    check(KxCfgDeleteConfiguration(path,NULL),"delete with foreign debugger");
    size=sizeof(value); error=RegQueryValueExW(key,L"Debugger",NULL,&type,(BYTE*)value,&size);
    check(!error && !lstrcmpW(value,L"foreign-debugger.exe"),"foreign debugger preserved");
    RegDeleteValueW(key,L"Debugger"); RegCloseKey(key);
    KxCfgDeleteConfiguration(path,NULL);
    printf("failures=%d\n",failures); fflush(NULL); ExitProcess(failures);
}
