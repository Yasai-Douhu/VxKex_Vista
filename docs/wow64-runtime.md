# WOW64 runtime deployment for Vista / Server 2008 x64

The file VSCodeUserSetup-x64-1.70.2.exe has PE machine 0x014c (x86), even though
its payload targets x64. The VM already had its VxKex IFEO settings enabled,
including verifier flags 0x80000000 and Windows 10 spoofing, but neither
SysWOW64/KexDll.dll nor C:/VxKex/Kex32 was installed.

The installer now requires and installs 13 x86 runtime DLLs from Installer/Kex32
into C:/VxKex/Kex32 and %windir%/SysWOW64. The existing x64 runtime and property
sheet remain in their original locations. Uninstall removes these explicit x86
filenames. No new global application configuration is enabled.

Build with build_runtime_x86.ps1 using VS2010 and SDK 7.0A/7.1. The x86 base/NT
scripts now select msvcrt_x86.lib. The extended runtime build explicitly links
x86 helper libraries, excludes x64 assembly, and stops on compilation/link errors.
package_x86.ps1 verifies the PE architecture before copying each DLL.

On 2026-09-20 the x86 runtime was deployed to the Server 2008 VM. All 13 binaries
passed PE architecture checks; imports were inspected for accidental MSVC runtime
redistributable dependencies. Application startup is pending user validation.

The user can run Desktop/Check-Wow64.cmd to collect the 32-bit probe result in
C:/VxKexProbe/Wow64/probe-result.txt, and launch C:/VSCodeUserSetup-x64-1.70.2.exe.
The probe has its own IFEO configuration and reports WOW64, automatic KexDll
loading, GetVersionEx and RtlGetVersion. It does not manually load KexDll.
No automated UI or guest feature tests were performed.

This deployment removes the missing-provider blocker. It does not establish
compatibility for every 32-bit application or every child-process combination.
In particular, x86 dwrw10.dll is not yet packaged for apps using the optional
KxDw DirectWrite replacement. The target installer must still be checked by
the user, including its extracted child process.

## Separate Vista IFEO registry views

The user's first probe after DLL deployment still returned KexDllLoaded=0,
GetVersionEx=6.0.6003, RtlGetVersion=6.0.6003 and exit code 1.
Inspection using System32/reg.exe and SysWOW64/reg.exe showed that the probe
and installer keys existed only in the native (64-bit) IFEO view.

Configuration reading, creation and removal now select the 32-bit view for
32-bit PE targets on NT 6.0. Other OS versions keep their existing lookup.
The installer also creates the propagation virtual key in the WOW64 view.
The rebuilt x64 KexCfg and property sheet were deployed and hash-verified;
both target configurations were read back successfully with SysWOW64/reg.exe.
Explorer may retain the previous property sheet until it is restarted.
The former native-view settings were left intact. General migration of old
settings and configuration-list enumeration across both views remain outstanding.

The installer was requested to launch normally through VMware guest execution,
as requested by the user. Successful launch request is not proof of UI success.
The second user-run probe and installer outcome are pending.

## Missing system.c in x86 build
The subsequent installer launch failed with STATUS_ENTRYPOINT_NOT_FOUND for
kernel32!K32GetProcessMemoryInfo. The x86 build omitted system.c, allowing the
linker to satisfy exports with kernel32 import-library thunks. Added system.c
and explicit undecorated x86 aliases for its three APIs. Build checks now reject
OS imports of those APIs and missing undecorated exports. Rebuilt successfully
and deployed KxBase.dll to guest SysWOW64 and C:/VxKex/Kex32; SysWOW64 readback
hash matched. Application behavior awaits user verification.


## Vista WOW64 child initialization

The user confirmed the direct x86 probe: KexDllLoaded=1, both version APIs
10.0.19045 and exit 0. The extracted installer .tmp then failed with 0xc0000005.
Guest SysWOW64/ntdll.dll NtOpenKey at RVA 0x49688 has this syscall tail:
`64 FF 15 C0 00 00 00 C2 0C 00` (call fs:[0xc0]; ret 12).
The propagation and restoration templates contained the Windows 7-only
`83 C4 04` (add esp,4) before ret. On Vista that discards the return address.
Both templates now replace that instruction with three NOPs on actual NT 6.0;
other operating systems retain the prior bytes. This corrects a concrete stack
imbalance; its relation to the observed installer crash awaits the user's rerun.

The x86 KexDll was rebuilt in an isolated output directory, deployed to SysWOW64
and C:/VxKex/Kex32, and the SysWOW64 readback SHA256 matched. The build supports
an output-directory argument and uses embedded debug information (/Z7) to avoid
shared compiler PDB conflicts. Existing helper-library PDB warnings remain.

Desktop/Check-Wow64-Child.cmd runs the new configured x86 parent which launches
VxKexChildNoIfeo.exe (the version probe under a new name). The child IFEO key
was confirmed absent. This tests inherited configuration and automatic provider
loading independently of Inno Setup. Expected: KexDllLoaded=1, both versions
10.0.19045, ChildExitCode=0x00000000 and ParentExitCode=0.
No diagnostic application or installer UI verification was run by the agent.

## Follow-up after child probe failure

User reported child KexDllLoaded=0, NT 6.0.6003, child exit 1, and the
installer still failed with 0xc0000005. The stack-tail fix alone did not resolve
propagation. Same-bitness children now use ProcessBasicInformation; the
ProcessWow64Information path is reserved for a native x64 caller inspecting
an x86 child. The Vista x86 path also clears IMAGE_KEY_MISSING before resume,
using explicit target-layout offsets (PEB32+0x10, parameters+0x08).
These changes are candidates pending runtime confirmation, not a confirmed
explanation of the installer failure.

The parent probe now reports its own provider presence, both process-info query
results, child process-parameter flags and the first six bytes of child NtOpenKey
before resuming the suspended child. This will distinguish missing parent
activation, hook installation and skipped IFEO lookup on the next user run.
Rebuilt and deployed the x86 provider plus parent probe; SysWOW64 provider
readback hash matched. No application verification was performed by the agent.

## Initialization-status diagnostic
User reported ParentKexDllLoaded=1, successful PEB queries returning the same
7efde000, unchanged child flags 0x4001 and original NtOpenKey bytes. Thus the
PEB lookup hypothesis was not supported by this run. Added a read-only getter
for the actual propagation initialization result and parent NtCreateUserProcess
entry bytes to the probe. Diagnostic x86 DLL and probe were deployed and hash
verified, without claiming a functional fix. Awaiting user-run status output.


## Private syscall stack fix
The user reported PropagationInitStatus=c0000005 and an unhooked parent
NtCreateUserProcess. Inspection found the same unconditional Win7 ADD ESP,4
in GENERATE_SYSCALL_WIN7's WOW64 wrapper, used by KexNtProtectVirtualMemory
during hook installation. The wrapper now skips this on actual NT 6.0.
Build succeeded; binary inspection found 16 syscall tails with the conditional
branch over ADD ESP,4. Deployed x86 DLL and verified SysWOW64 readback hash.
Runtime initialization and installer outcome remain pending user verification.


## User-confirmed installer startup milestone

The user confirmed the final child probe: propagation initialization 0, parent
and child hooks present, child flags 1, KexDllLoaded=1, both version APIs
10.0.19045, and parent/child exit 0. The user then confirmed that
VSCodeUserSetup-x64-1.70.2.exe reached its installer license page.
This establishes startup of the x86 bootstrapper and extracted child, not
successful installation or subsequent VS Code execution.

Screenshot shows missing button captions and upper explanatory text while the
license content and logo render. Rendering remains unresolved. Initial source
inspection found DrawText and LoadString in KxUser forward to user32; no evidence
yet identifies a particular resource or drawing API as the cause. No additional
runtime change or release publication was made on the basis of this screenshot.

## Command-based UI diagnostics authorized

User authorized command-based guest testing (no direct screen interaction).
Launched the installer with /LOG only; no installation was performed.
The log records "MsgBox failed" for the administrator warning. Themes service
is RUNNING; UxTheme.dll and both legacy and side-by-side COMCTL32 are loaded.
A noninvasive CDB x86 stack snapshot shows the primary thread in
USER32!WaitMessage. This is not evidence of a complete process deadlock;
message handling or dialog creation may still be faulty. Symbols are incomplete.
Logs: audit/wow64/installer-ui.log, ui-processes.txt, ui-stacks2.txt.
No source fix or release publication followed these observations. The second
installer instance remains open for further diagnostics; CDB detached with qd.

## Controls and message-box return captured

Command-based GUI probes with and without a common-controls v6 manifest
returned nonzero MessageBoxTimeout results, caption lengths 14, and destroyed
their own window after WM_CLOSE, both native and with VxKex. These do not
establish correct pixel rendering. Strong spoof disabled did not eliminate
installer MsgBox failures; the installer setting was restored to 3.

CDB captured the installer processing WM_CLOSE and returning from MessageBoxW
with EAX=0 and TEB LastErrorValue=0 (audit/wow64/trace-return.txt). Debugger
successfully detached. Initial attempts failed due to command-file parsing;
the successful run used redirected debugger input.

Enumerating installer PID 4072 found all expected controls with captions:
license heading and instructions, agree/disagree radio buttons, disabled Next,
and enabled Cancel. They have visible styles and expected enabled state.
Thus missing control creation is not supported by this observation; painting
or initialization of rendering state remains a candidate. No production DLL
change was made during these GUI diagnostics. The installer remains open;
no installation was performed. Control inventory: audit/wow64/controls.txt.

## Remote CDB: dialog failure narrowed (2026-09-22)

In child PID 2116, a command-based WM_CLOSE reaches MessageBoxW with valid
Japanese strings, owner HWND, and flags 0x124. SoftModalMessageBox allocates
its template successfully. The internal dialog builder creates the dialog
window, but NtUserCreateWindowEx returns NULL for the redirected common-control
class `6.0.6003.20560!Static` (control ID 0xcafe, style 0x50000000).
The dialog builder then destroys the dialog and returns -1; MessageBoxW returns
0 with LastErrorValue still 0. This explains the failed close confirmation,
but the reason for the Static creation failure is not yet established.

Extended gui-probe.c checks STATIC creation and an owned MB_YESNO question
dialog with a 200ms timeout. Native, directly enabled VxKex, and VxKex inherited
from a parent all succeeded: StaticTextLength=14, OwnedQuestionBox=0x7d00
(timeout), WindowAfterClose=0. These are command-level results, not pixel
rendering checks. Evidence: audit/wow64/static-{native,kex,child}.txt.
Thus propagation alone does not reproduce the installer-specific failure.

The actual extracted installer body was imported and analyzed in the existing
Ghidra project as /VSCodeUserSetup-x64-1.70.2.tmp. No production DLL changes
or release publication were made during this investigation.

## Inno manifest reproducer (2026-09-22)

Adding a static COMCTL32 import/InitCommonControls call and the Inno 6.0.5
XPTheme.manifest to the small GUI probe reproduces failed BUTTON and STATIC
creation (ERROR_CANNOT_FIND_WND_CLASS=1407) and MessageBox return 0, even with
KexDllLoaded=0. Returning to the simple common-controls manifest succeeds with
the same statically linked binary. This is evidence from this Server 2008 VM,
not a claim about every Vista installation or every Inno Setup version.

Removing only the top-level file elements from the Inno manifest restores
BUTTON/STATIC creation, caption lengths 14, and an owned question message box
(0x7d00 timeout), both native and with VxKex enabled. Removing only DPI,
compatibility, or trustInfo does not restore them. Keeping any single original
file/loadFrom element (mpr, netapi32, netutils, or version) reproduces failure
natively. Removing just loadFrom from the version element restores native
controls; that variant's VxKex run crashed before producing output and is not
a viable fix. Absolute System32/SysWOW64 directory variants still fail.
Full-filename loadFrom experiments did not complete cleanly and have no valid
comparative result; later files in that run may contain stale output.

Evidence: audit/wow64/inno-manifest-{native,kex}.txt,
comctl-static-{native,kex}.txt, manifest-No*.txt, file-*.txt,
no-loadfrom-native.txt and no-files-{native,kex}.txt. The initial NoFiles
resource update failed; its result was discarded and replaced by a freshly
linked, successfully embedded VxGuiNoFilesFresh.exe run.

The Inno loadFrom entries are DLL search-path hardening. Do not ship their
unconditional removal: any VxKex workaround must preserve trusted system DLL
resolution. The precise activation-context failure and original installer's
pixel rendering after a workaround remain unverified. Only diagnostic EXEs
were changed/deployed; production binaries and the original installer remain
unchanged. Diagnostic processes that crashed in the incomplete path experiments
were terminated; the existing installer process was left running.
