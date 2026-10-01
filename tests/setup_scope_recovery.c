// Explicit recovery of fixtures retained by the standard-token launch test.
#define mainCRTStartup OriginalScopeMain
#include "setup_user_scope_probe.c"
#undef mainCRTStartup
VOID __cdecl mainCRTStartup(VOID)
{
    LPUSER_INFO_1 Account = NULL; HANDLE Token = NULL; PROFILEINFO Profile = {sizeof(Profile)};
    BYTE SidBytes[SECURITY_MAX_SID_SIZE]; DWORD Size = sizeof(SidBytes), DomainSize = 128;
    WCHAR Domain[128], Path[MAX_PATH]; SID_NAME_USE Use; PWSTR Sid = NULL; BOOL Loaded = FALSE, GuiFixture = FALSE;
    log = fopen("C:\\VxKexProbe\\NextParity\\setup-scope-recovery.txt", "wt"); if (!log) ExitProcess(2);
    check(guard() && !KxCfgpElevationRequired(), "authorized disposable VM and elevated recovery required"); if (failures) goto Done;
    check(!NetUserGetInfo(NULL, USERNAME, 1, (LPBYTE*)&Account) && Account &&
        Account->usri1_comment && !wcscmp(Account->usri1_comment, L"VxKex parity disposable user scope test"),
        "recover only exact test account with recorded creation comment"); if (failures) goto Done;
    check(LookupAccountName(NULL, USERNAME, SidBytes, &Size, Domain, &DomainSize, &Use) &&
        ConvertSidToStringSid(SidBytes, &Sid), "resolve retained test SID"); if (failures) goto Done;
    check(LogonUser(USERNAME, L".", L"Parity_Clone_261001!", LOGON32_LOGON_NETWORK, LOGON32_PROVIDER_DEFAULT, &Token),
        "authenticate owned test account"); if (failures) goto Done;
    check(privilege(SE_BACKUP_NAME) && privilege(SE_RESTORE_NAME), "enable profile recovery privileges"); if (failures) goto Done;
    Profile.dwFlags = PI_NOUI; Profile.lpUserName = USERNAME;
    Loaded = LoadUserProfile(Token, &Profile); check(Loaded, "load only retained test hive"); if (failures) goto Done;
    Size = ARRAYSIZE(Path);
    check(GetUserProfileDirectory(Token, Path, &Size) && !_wcsicmp(Path, EXPECTED_PROFILE), "exact profile path required before removal"); if (failures) goto Done;
    {
        HKEY Key; ULONG Enabled; WCHAR Directory[MAX_PATH], Expected[MAX_PATH];
        ExpandEnvironmentStrings(L"%USERPROFILE%\\VxKex GUI \u4fdd\u5b58\\", Expected, ARRAYSIZE(Expected));
        if (!RegOpenKeyEx(HKEY_CURRENT_USER, PRODUCT, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Key)) {
            if (!RegReadI32(Key, NULL, L"EnableLogging", &Enabled) && Enabled == 1) {
                check(!RegReadString(Key, NULL, L"LogDir", Directory, ARRAYSIZE(Directory)) &&
                    (!wcscmp(Directory, L"C:\\VxKex\\Logs") || !wcscmp(Directory, Expected)),
                    "recover only precisely recorded failed GUI-save preferences");
                if (!failures) {
                    check(!RegWriteI32(Key, NULL, L"EnableLogging", 0) && !RegDeleteValue(Key, L"LogDir"), "restore exact owned operator fixture");
                    GuiFixture = TRUE;
                }
            }
            RegCloseKey(Key);
        }
    }
    {
        PCWSTR Conflict = L"Software\\Classes\\Msi.Package\\shell\\open_vxkex";
        HKEY Key; WCHAR Label[128]; LONG Error;
        Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, Conflict, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &Key);
        if (!Error) {
            check(!RegReadString(Key, NULL, NULL, Label, ARRAYSIZE(Label)) && !wcscmp(Label, L"VxKex parity foreign menu fixture"),
                "recover only exact owned foreign-menu fixture");
            if (!failures) check(!RegDeleteValue(Key, NULL), "remove exact injected conflict label"); RegCloseKey(Key);
            if (!failures) check(!RegDeleteKeyEx(HKEY_LOCAL_MACHINE, Conflict, KEY_WOW64_64KEY, 0), "remove empty owned conflict key");
        } else check(Error == ERROR_FILE_NOT_FOUND, "no unrelated conflict key adopted by recovery");
    }
    preference(HKEY_CURRENT_USER, 0, FALSE); preference(Profile.hProfile, 1, FALSE); if (failures) goto Done;
    check(!setup(FALSE, Sid), "remove retained diagnostic installation with exact user SID"); if (failures) goto Done;
    check(!exists(Profile.hProfile), "selected test preferences removed");
    check(!RegDeleteKeyEx(HKEY_CURRENT_USER, PRODUCT, KEY_WOW64_64KEY, 0), "remove operator preferences created by retained test");
    if (GuiFixture) check(!RegDeleteTree(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\\VxKex Log Files"), "remove cleanup handler created by failed GUI save after verified initial absence");
Done:
    if (Loaded) {
        check(UnloadUserProfile(Token, Profile.hProfile), "unload test hive before profile removal");
        if (!failures) {
            check(DeleteProfile(Sid, Path, NULL), "delete verified exact test profile");
            if (!failures) check(!NetUserDel(NULL, USERNAME), "delete verified owned test account");
        }
    }
    if (Account) NetApiBufferFree(Account); if (Sid) LocalFree(Sid); if (Token) CloseHandle(Token);
    fprintf(log, "Failures=%u\n", failures); fclose(log); ExitProcess(failures ? 1 : 0);
}
