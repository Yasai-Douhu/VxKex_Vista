# Filename-independent Inno Setup compatibility on NT 6.0

## Cause and upstream comparison (2026-09-29)

Sublime Text 4215 failed with Runtime error 255 at 00DC58B7, while renaming
the identical file to the supported 4213 basename allowed it to start.
The former `AshApplyGitInstallerAvxWorkaround` explicitly enumerated Git,
Sublime 4213 and VS Code filenames, then checked exact timestamps, image sizes,
entry points and patch RVAs. Sublime 4213 and 4215 use the same outer Inno
engine identity and initialization code; the filename alone excluded 4215.

The pre-change live dump confirms exception C000001D at 00DC58B7 (RVA 58B7),
`vmovups ymm1, [eax]`. Delphi enables AVX after a spoofable Windows 7 SP1
version check and a CPU capability check, without checking OSXSAVE/XCR0.
The real OS is Server 2008 / NT 6.0 and cannot execute this path. Microsoft
documents [AVX API support beginning with Windows 7 SP1 / Server 2008 R2 SP1](https://learn.microsoft.com/en-us/windows/win32/debug/working-with-xstate-context).

The current upstream source was inspected at commit
`f98d85759c0e5c40051d5a28339fee1a2bfb2e8d` (1.2.3.2463).
Its [ash.c](https://github.com/YuZhouRen86/VxKex-NEXT/blob/f98d85759c0e5c40051d5a28339fee1a2bfb2e8d/KexDll/ash.c)
recognizes Qt6, Godot and Zig through PE section content; it also retains
filename-based workarounds for other applications. It has no equivalent of
our Vista Inno AVX patch. Its
[loader](https://github.com/YuZhouRen86/VxKex-NEXT/blob/f98d85759c0e5c40051d5a28339fee1a2bfb2e8d/KexDll/kexldr.c)
rewrites supported DLL names through common loader mechanisms. Windows 7 SP1
has OS support for AVX, so a Vista-specific workaround cannot simply be
copied from upstream. We adopted content-based recognition and PE metadata
instead of extending the per-filename list.

## Implementation

`AshApplyInnoSetupWorkarounds` still requires real NT 6.0 and enabled
application-specific workarounds. It identifies the x86 Inno engine through
its resource attribution. `00-Common-Headers/InnoProfile.h` validates:

- the mapped PE32 image and section ranges;
- the OS-gated CPUID test followed by the AVX-flag initialization;
- writable in-image locations for the capability byte and dispatch flag;
- an executable consumer testing that same flag and branching to the AVX
  copy implementation;
- a unique initialization match, rejecting ambiguous matches.

It then disables only that AVX dispatch, retaining the existing SSE path.
No filename, timestamp, build number, image size constant or patch RVA is
used for selection. Relocated addresses are interpreted against the loaded
image base. Unknown code patterns are not patched.

USER32 delay-load redirection now reads `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT`
descriptors (RVA and legacy VA forms), validates bounds and redirects their
USER32 name to KxUser. It no longer depends on body-specific string offsets.
The existing content-based common-controls registration uses the same Inno
attribution recognizer. Changes apply to loaded memory; installer files are
not patched on disk.

## Verification and delivery

`tests/inno_profile_probe.c` maps PE data without executing it. Positive tests
passed for Sublime 4213, Sublime 4215, a reconstructed 4213 body, Git 2.55.0.5,
and VS Code 1.139.1. The same scanner rejects missing attribution, invalid
flag addresses, truncated input, modified instruction bytes and ambiguous
initializers; changing the PE timestamp does not affect recognition.
Both x86 and x64 KexDll builds passed. This particular runtime patch remains
x86-only, matching the actual outer loaders and setup bodies in these tests.

Server 2008 runtime checks:

| Input | Evidence |
| --- | --- |
| Original 4215 filename | Final setup log records original Downloads path; responsive wizard, labels and buttons visible |
| Same 4215 bytes named `inno-content-rename.exe` | Setup log records new path; responsive wizard |
| Archived 4213 named `inno-legacy-content.exe` | Setup log and responsive 4213 wizard |
| Git 2.55.0.5 | Inno 7.0.2 log; responsive wizard |
| VS Code 1.139.1 | Inno 6.4.1 log; responsive wizard after acknowledging its administrator warning |
| WOW64 propagation | Parent/child KexDll loaded, version 10.0.19045, both exit codes 0 |

The final screenshot `audit/inno4215-final.png` shows correctly drawn text, checkbox,
Next and Cancel. Final evidence is in `audit/inno-windows-final.txt`,
`audit/inno4215-final-setup.log`, `audit/inno-legacy-setup.log`,
`audit/inno-rename-setup.log`, `audit/inno-git-final.log`,
`audit/inno-code-final.log`, `audit/inno-final-regression.txt`, and
`audit/inno4215-exception.log`. No application installation was performed.

The final x86 DLL was deployed to Server 2008's SysWOW64 and VxKex/Kex32,
and copied to Installer/Kex32/KexDll.dll. Downloaded-back SysWOW64 and
Installer copies have SHA256
`C4955EAB1565191575513295D278354307CA387CBC2C2E54141209DE132E48F8`.
Previous guest DLLs are retained as `KexDll.pre-inno-generic-20260929.dll`.
The final 4215 wizard remains open for inspection. Temporary test settings
are removed after verification.

This verifies filename-independent handling of the recognized Delphi engine
family; it does not promise support for every future compiler instruction
sequence, modified attribution resource, native x64 Inno engine, or a complete
installation of each tested application.
