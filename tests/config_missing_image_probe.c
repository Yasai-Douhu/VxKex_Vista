/* Regression: missing WOW64 image and same basename in both IFEO views.
   Uses only dedicated fixtures; refuses to overwrite an existing test key. */
#include "../KxCfgHlp/buildcfg.h"
#include <KexComm.h>
#include <KxCfgHlp.h>
#include <stdio.h>

static const WCHAR image64[] = L"C:\\VxKexProbe\\NextParity\\NextParityMissingImageFixture-20261001.exe";
static const WCHAR image32[] = L"C:\\VxKexProbe\\NextParity\\x86\\NextParityMissingImageFixture-20261001.exe";
static const WCHAR backup64[] = L"C:\\VxKexProbe\\NextParity\\NextParityMissingImageFixture-20261001.bak";
static const WCHAR backup32[] = L"C:\\VxKexProbe\\NextParity\\x86\\NextParityMissingImageFixture-20261001.bak";
static const WCHAR keyPath[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\NextParityMissingImageFixture-20261001.exe";
static FILE *log;
static int failures;
static void check(BOOL condition, const char *name)
{
    fprintf(log, "%s %s error=%lu\n", condition ? "PASS" : "FAIL", name, GetLastError());
    fflush(log);
    if (!condition) ++failures;
}
VOID __cdecl mainCRTStartup(VOID)
{
    HKEY key32 = NULL, key64 = NULL;
    LONG error;
    DWORD value, bytes, type;
    WCHAR path[MAX_PATH];
    KXCFG_PROGRAM_CONFIGURATION config = {0}, readback;
    BOOL moved32 = FALSE, moved64 = FALSE, created = FALSE;
    log = fopen("C:\\VxKexProbe\\NextParity\\missing-image-result.txt", "wt");
    if (!log) ExitProcess(2);
    error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ | KEY_WOW64_32KEY, &key32);
    check(error == ERROR_FILE_NOT_FOUND, "fresh 32bit fixture key");
    if (!error) RegCloseKey(key32);
    key32 = NULL;
    error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ | KEY_WOW64_64KEY, &key64);
    check(error == ERROR_FILE_NOT_FOUND, "fresh 64bit fixture key");
    if (!error) RegCloseKey(key64);
    key64 = NULL;
    if (failures) goto Done;
    config.Enabled = TRUE;
    config.WinVerSpoof = WinVerSpoofWin10;
    created = TRUE;
    check(KxCfgSetConfiguration(image64, &config, NULL), "configure x64 fixture");
    check(KxCfgSetConfiguration(image32, &config, NULL), "configure x86 fixture");
    if (failures) goto Cleanup;
    error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ | KEY_WRITE | KEY_WOW64_32KEY, &key32);
    check(!error, "open x86 key");
    if (error) goto Cleanup;
    value = 123;
    check(!RegSetValueExW(key32, L"UnrelatedFixtureValue", 0, REG_DWORD, (BYTE *)&value, sizeof(value)), "seed unrelated value");
    {
        const WCHAR foreignVerifiers[] = L"prefixkexdll.dll KEXDLL.DLL kexdll.dll.backup Other.DLL";
        DWORD flags = 0x120;
        check(!RegSetValueExW(key32, L"VerifierDlls", 0, REG_SZ,
            (BYTE *)foreignVerifiers, sizeof(foreignVerifiers)), "seed foreign verifier tokens");
        check(!RegSetValueExW(key32, L"GlobalFlag", 0, REG_DWORD,
            (BYTE *)&flags, sizeof(flags)), "seed shared verifier flags");
    }
    moved32 = MoveFileW(image32, backup32);
    check(moved32, "remove x86 image from original path");
    if (!moved32) goto Cleanup;
    check(KxCfgDeleteConfiguration(image32, NULL), "delete missing x86 configuration");
    check(KxCfgGetConfiguration(image64, &readback) && readback.Enabled, "same basename x64 stays enabled");
    bytes = sizeof(path);
    check(RegQueryValueExW(key32, L"KEX_ConfigPath", NULL, &type, (BYTE *)path, &bytes) == ERROR_FILE_NOT_FOUND,
        "x86 metadata removed");
    bytes = sizeof(value); value = 0;
    check(!RegQueryValueExW(key32, L"UnrelatedFixtureValue", NULL, &type, (BYTE *)&value, &bytes) && value == 123,
        "unrelated x86 value preserved");
    bytes = sizeof(path);
    check(!RegQueryValueExW(key32, L"VerifierDlls", NULL, &type, (BYTE *)path, &bytes) &&
        !lstrcmpW(path, L"prefixkexdll.dll  kexdll.dll.backup Other.DLL"), "foreign verifier names preserved");
    bytes = sizeof(value); value = 0;
    check(!RegQueryValueExW(key32, L"GlobalFlag", NULL, &type, (BYTE *)&value, &bytes) && value == 0x120,
        "shared verifier and unrelated flags preserved");
    RegDeleteValueW(key32, L"VerifierDlls");
    RegDeleteValueW(key32, L"GlobalFlag");
    RegDeleteValueW(key32, L"VerifierFlags");
    if (MoveFileW(backup32, image32)) moved32 = FALSE;
    check(!moved32, "restore x86 image");
    if (moved32) goto Cleanup;
    check(KxCfgSetConfiguration(image32, &config, NULL), "reconfigure x86 fixture");
    value = 0x102;
    check(!RegSetValueExW(key32, L"GlobalFlag", 0, REG_DWORD, (BYTE *)&value, sizeof(value)),
        "seed unrelated loader snaps flag");
    check(KxCfgDeleteConfiguration(image32, NULL), "delete sole VxKex verifier");
    bytes = sizeof(value); value = 0;
    check(!RegQueryValueExW(key32, L"GlobalFlag", NULL, &type, (BYTE *)&value, &bytes) && value == 2,
        "loader snaps flag preserved");
    RegDeleteValueW(key32, L"GlobalFlag");
    check(KxCfgSetConfiguration(image32, &config, NULL), "restore fixture before ambiguity test");
    check(!RegSetValueExW(key32, L"KEX_ConfigPath", 0, REG_SZ, (BYTE *)image64, sizeof(image64)), "seed ambiguous paths");
    moved64 = MoveFileW(image64, backup64);
    check(moved64, "remove x64 image for ambiguity test");
    if (!moved64) goto Cleanup;
    SetLastError(0);
    check(!KxCfgDeleteConfiguration(image64, NULL) && GetLastError() == ERROR_DUP_NAME, "ambiguous deletion rejected");
    bytes = sizeof(path);
    check(!RegQueryValueExW(key32, L"KEX_ConfigPath", NULL, &type, (BYTE *)path, &bytes) && !lstrcmpW(path, image64),
        "ambiguous x86 metadata unchanged");
    error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ | KEY_WOW64_64KEY, &key64);
    check(!error, "ambiguous x64 key preserved");
    if (!error) {
        bytes = sizeof(path);
        check(!RegQueryValueExW(key64, L"KEX_ConfigPath", NULL, &type, (BYTE *)path, &bytes) && !lstrcmpW(path, image64),
            "ambiguous x64 metadata unchanged");
        RegCloseKey(key64); key64 = NULL;
    }
Cleanup:
    if (moved32) check(MoveFileW(backup32, image32), "cleanup restore x86 image");
    if (moved64) check(MoveFileW(backup64, image64), "cleanup restore x64 image");
    if (key32) {
        HKEY foreign = NULL;
        RegSetValueExW(key32, L"KEX_ConfigPath", 0, REG_SZ, (BYTE *)image32, sizeof(image32));
        RegDeleteValueW(key32, L"UnrelatedFixtureValue");
        RegDeleteValueW(key32, L"VerifierDlls");
        RegDeleteValueW(key32, L"GlobalFlag");
        RegDeleteValueW(key32, L"VerifierFlags");
        error = RegCreateKeyExW(key32, L"ForeignState", 0, NULL, 0,
            KEY_READ | KEY_WRITE, NULL, &foreign, NULL);
        check(!error, "create foreign fixture subkey");
        if (!error) {
            value = 456;
            check(!RegSetValueExW(foreign, L"Marker", 0, REG_DWORD,
                (BYTE *)&value, sizeof(value)), "seed foreign subkey value");
            check(KxCfgDeleteConfiguration(image32, NULL), "delete disabled fixture configuration");
            bytes = sizeof(value); value = 0;
            check(!RegQueryValueExW(foreign, L"Marker", NULL, &type, (BYTE *)&value, &bytes) && value == 456,
                "foreign subkey preserved");
            RegDeleteValueW(foreign, L"Marker");
            RegCloseKey(foreign);
            check(!RegDeleteKeyW(key32, L"ForeignState"), "cleanup foreign fixture subkey");
        }
        RegCloseKey(key32);
    }
    if (created) {
        check(KxCfgDeleteConfiguration(image32, NULL), "cleanup x86 configuration");
        check(KxCfgDeleteConfiguration(image64, NULL), "cleanup x64 configuration");
    }
Done:
    fprintf(log, "Failures=%d\n", failures);
    fclose(log);
    ExitProcess(failures ? 1 : 0);
}
