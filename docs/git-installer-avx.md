# Git 2.55.0.5 installer compatibility profile

The historical per-filename/RVA profile below was replaced on 2026-09-29 by
the [content-based Inno profile](inno-content-profile.md).

## Current result (2026-09-26)

The original installer now reaches its visible, enabled `Git 2.55.0.5 Setup`
window on Server 2008. Command-based enumeration found the Information page,
Next and Cancel buttons. A bounded WM_GETTEXT request returned the correct
title immediately (error 0, elapsed 0 ms); IsHungAppWindow returned false.
No installation was performed. Pixel rendering and later installation pages
have not been verified. This profile matches only the inspected Git build;
it does not establish a fix for the older VS Code installer.

The x86 build, both deployed VM DLLs (SysWOW64 and C:\VxKex\Kex32), and
Installer/Kex32/KexDll.dll have identical SHA256:
`f181ecbd96787117666ec7c03eff3743ac600ce51cb281f021d46270a2bb29bf`.
WOW64 parent/child regression passed: KexDll loaded in both, propagation status
0, child GetVersionEx/RtlGetVersion 10.0.19045 and child exit 0.
Evidence: audit/git-controls.txt, audit/git-controls-setup.log,
audit/git-controls-regression.txt. No release was published.

## Initial failure

Inspected the original installer copied from the Server 2008 VM on 2026-09-24.
SHA256: `6c12e9345067f8e9172aa1565c9de27d6b10bd824185e80d234afbf7b517c7a4`.
The outer executable is x86 (PE machine 0x14c), image base 0x00400000,
timestamp 0x6a5222db, matching the supplied crash report.

The reported RVA 0x58b7 maps exactly to bytes `c5 fc 10 08`, decoded as
`vmovups ymm1, ymmword ptr [eax]`: an AVX memory-copy instruction.
This establishes a different immediate failure from the VS Code installer's
themed control creation failure.

The dispatch at VA 0x405840 tests bit 0 of the global at 0x4b6060 and jumps
to the faulting AVX implementation when set. An SSE implementation is present
in the alternate branch.

Initialization at 0x405724 calls 0x405710, which passes version 6.1 SP1 to
0x405684. That function constructs an OSVERSIONINFOEXW and calls
VerifyVersionInfoW (through 0x403958), using VerSetConditionMask
(through 0x403950). If the version test succeeds, initialization reads bit 28
of the cached CPUID leaf 1 ECX at 0x4b6918 and enables the AVX path.
The CPU cache initialization at 0x405ef8 reads CPUID leaves 0 through 7.
The examined AVX dispatch initialization does not test OSXSAVE/XCR0.

Therefore version spoofing can satisfy the OS gate while the real NT 6.0
environment cannot execute AVX. The controlled in-memory dispatch override
described below allowed both installer stages to pass this failure. The
original installer file on disk remains unchanged.

Microsoft documents Windows 7 SP1 / Server 2008 R2 SP1 as the first Windows
versions supporting the AVX API:
https://learn.microsoft.com/en-us/windows/win32/debug/working-with-xstate-context

A potential compatibility workaround is to keep this runtime's memory-copy
dispatch on its existing SSE path. This would address the reported exception,
not establish that subsequent setup UI or installed Git binaries work.

## AVX workaround and intermediate result

AshApplyGitInstallerAvxWorkaround now replaces the AVX initialization's
`setne al` with `xor al,al; nop` in memory on real NT 6.0 only. It honors
DisableAppSpecific and requires the exact basename, x86 PE timestamp, image
size, entry point, original instruction bytes, AVX instruction bytes and
relocated flag-store destination. Unsupported binaries are left untouched.

Two profiles are required: the outer EXE above and the extracted .tmp body
(timestamp 0x6a5222df, size 0x44e000, entry RVA 0x3af908, patch RVA 0x7438,
AVX RVA 0x75b7, dispatch flag RVA 0x3bc064).

The updated x86 KexDll was built and deployed to the VM's SysWOW64 and
VxKex/Kex32 runtime directories, and copied to Installer/Kex32.
SHA256: 22d6d64f64c2b2d3a2f436a174294a36575d582dd48afbad58f3fd146ab0398e.
The downloaded-back SysWOW64 DLL matches the build. Pre-change DLLs are
preserved in the guest under KexDll.pre-git-avx-20260924.dll names.

Command-based launch passed both AVX initialization stages and wrote an
Inno Setup 7.0.2 startup log. It subsequently exited with a different error:
`Error reading FComponentsList.Offset: External exception C06D007F.`
The setup UI and installation therefore remain unverified; no installation
was performed. See audit/git-avx-setup.log. The WOW64 parent/child regression
still reports both KexDll loaded, version 10.0.19045 and child exit 0
(audit/git-avx-regression.txt). No release was published for this change.

## Subsequent startup failures and fixes

1. The C06D007F dump identified Delphi delay loading of
   `user32.dll!GetWindowDpiAwarenessContext`. The regular import rewrite did not
   cover this path. The child profile checks and replaces the shared delay DLL
   name at RVA 0x3cd7d6 with `kxuser.dll`, using the existing DPI adapters.
2. The next failure was `List item and state item count mismatch.` Native
   control probes reproduced class creation failure with the installer's
   loadFrom manifest entries: InitCommonControls bound to the system legacy
   COMCTL32, despite the active context selecting common-controls v6.
   Loading v6 and calling InitCommonControls alone was insufficient; explicitly
   calling its RegisterClassNameW for the standard control classes succeeded.
3. The child profile validates and hooks the InitCommonControls thunk at RVA
   0x18930. At normal application initialization, the helper calls the original
   resolved IAT target, queries the active context's assembly directory, loads
   that WinSxS COMCTL32 and registers Static, Button, ListBox, ComboBox, Edit and
   ScrollBar. The servicing version is selected by the OS. The module remains
   loaded for its registered window procedures. The original manifest remains
   intact; no system DLL is replaced.

All three workarounds are scoped to the exact x86 EXE/body PE identities on
real NT 6.0 and honor DisableAppSpecific. Matching code/data bytes are checked
before patching. Guest pre-change runtime backups are retained, including
KexDll.pre-controls-20260926.dll. These are startup compatibility workarounds,
not a claim that the installed Git binaries are compatible with Vista.
