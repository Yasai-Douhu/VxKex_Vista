# VS Code 1.139.1 installer configuration (2026-09-27)

## Runtime follow-up (resolved)

The reported address 00E77467 corresponds to the same outer-image RVA 7467
when loaded at 00E70000. The x86 exact-build AVX profiles now include:

| Image | Timestamp | Size | Entry | Patch | AVX | Flag |
| --- | --- | --- | --- | --- | --- | --- |
| 1.139.1.exe | 67ac374c | dc000 | a7f98 | 72e0 | 7467 | afb98 |
| 1.139.1.tmp | 67ac374c | 363000 | 2cf9a8 | 7500 | 7687 | 2dcb9c |

After the outer fix, module enumeration of the live extracted body (base
001a0000) and Runtime error 255 at 001a7687 located the second AVX instruction.
Original instruction bytes and relocated dispatch flag destinations are
validated before applying the existing SSE-selection workaround.

After both fixes, setup proceeds into startup logging. A subsequent C06D007F
delay-load error was bypassed by routing the body's USER32 DPI delay-import
name at RVA 2e979e to KxUser, as in the Git/Sublime profiles.

The subsequent failure was `Failed to expand shell folder constant "userpf"`,
with SHGetKnownFolderPath returning 80070002. KxUser/knownfld.c now supplies
UserProgramFiles and UserProgramFilesCommon beneath the requested user's native
LocalAppData when the native folder query fails with an unsupported/missing
folder error. Other folder queries retain native behavior. Returned strings use
CoTaskMemAlloc; CREATE and DONT_VERIFY control directory creation/verification.

Inno dynamically resolves the native shell32 export, so adding the KxUser export
alone was insufficient. On real NT 6.0, KxBase's GetProcAddress wrapper now routes
that one named shell32 API to KxUser loaded from the configured absolute Kex
runtime directory. Ordinal and unrelated lookups remain native.

Validation on Server 2008 (2026-09-27):
- Normal launch of the original 1.139.1 EXE reaches the license wizard after
  acknowledging the known User Installer administrator warning.
- audit/vscode139-success.png shows the complete wizard with text and controls.
- audit/vscode139-response.txt: WM_NULL_response=1, Error=0 (native x64 probe).
- audit/vscode139-knownfolder2.log contains no userpf failure.
- audit/vscode139-knownfolder-regression.txt: child KexDllLoaded=1,
  both version APIs return 10.0.19045, ChildExitCode=0.
- No license acceptance or installation was performed.
- Built, deployed and Installer/Kex32 KxBase/KxUser SHA256 hashes match.
- The broad flags/token combinations of the new folder implementation and an
  x64 build of that implementation have not received separate runtime tests.

API references:
- https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ne-shlobj_core-known_folder_flag

Runtime profiles were built/deployed to SysWOW64 and C:\VxKex\Kex32 and copied
to Installer/Kex32. Build/deployed hashes match. The WOW64 child regression
still passes (audit/vscode139-final-regression.txt). No installation occurred.

The user's settings were correctly written to the WOW64 IFEO view, including
VerifierDlls and version 10 spoofing, but no Debugger launch entry was present.
The PE is x86 with subsystem requirement 6.1. Its ProductName is `Visual Studio
Code` padded with trailing spaces; the old exact product-name match failed.
This caused rejection before KexDll could be injected.

VistaRequiresSubsystemLauncher replaces the one-build Sublime test. On real
NT 6.0, enabled x86/AMD64 PE images requiring a subsystem newer than 6.0 now
receive the managed VistaRun IFEO launcher regardless of filename, product name
or timestamp. Existing VS Code application launch behavior and debugger
ownership checks are retained. This does not normalize product names or add
Electron command-line flags to this setup.

KexCfg and the shell extension were rebuilt, deployed to C:\VxKex, and copied
to Installer staging. LoadLibrary of the new shell DLL succeeds. Disable and
enable both return zero; the managed debugger is removed and recreated (see
audit/vscode139-config-test.txt). The invalid-Win32-application rejection no
longer occurs during normal launch. Explorer may retain a previously loaded
shell DLL until it is restarted; the target's current settings were reapplied
using the updated KexCfg.

