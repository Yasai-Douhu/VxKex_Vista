# Sublime Text build 4213 installer on Server 2008

## Drawing fix verified by screenshot (2026-09-27)

The user subsequently confirmed missing labels and blank controls. Window
enumeration alone had not established correct drawing. Extending the Git
standard-class registration hook to the inspected Sublime body fixed the
visible destination page: heading, descriptive labels, destination path,
Browse, Next and Cancel are drawn in `audit/sublime-controls-after.png`.
This is a VMware framebuffer screenshot, not a synthetic rendering. The
initial automated before capture was obscured by old dialogs; the user's
supplied screenshot is the clear pre-change comparison.

Sublime's validated InitCommonControls thunk is RVA 0x19008, targeting IAT
RVA 0x3c4bac. The shared helper now calls the per-profile IAT; Git retains
its original thunk/IAT. Original instruction bytes and relocated IAT
destination are checked before installing the hook. This patch is deployed
to the VM and Installer/Kex32; the build and downloaded deployed DLL match.
The WOW64 parent/child regression still passes. The wizard remains open;
no installation or later-page interaction was performed. VS Code has not
received this drawing workaround yet. Earlier deployment hashes below
describe the pre-drawing-fix build.

## Result (2026-09-27)

The original `C:\Users\Administrator\Downloads\sublime_text_build_4213_x64_setup.exe`
now launches normally with VxKex enabled. The final setup log records Windows
10.0.19045. Command-based window enumeration confirms a visible, enabled,
responsive setup wizard with Browse, Next and Cancel controls. No installation
was performed, and the installed editor's compatibility is outside this test.

Original installer SHA256:
`86202139d45974df77a9cc86aec1a6507a38797999221f4784cfce26d67390fc`.
Despite the filename, the installer loader and extracted setup body are x86.

## Causes and changes

1. Its PE subsystem version is 6.1. Configuration now selects the existing
   VistaRun IFEO launcher for this exact PE identity on NT 6.0. No VS Code
   command-line switches are applied to Sublime. Existing debugger ownership
   protections still apply; disabling removes the managed launcher.
2. Both loader and body use spoofable OS version information to enable an AVX
   memory-copy path unsupported by the real OS. A live dump confirms
   c000001d at the body's RVA 0x75b7 (`vmovups ymm1,[eax]`). The inspected-build
   profile keeps the existing SSE path, with the same instruction and relocated
   destination checks as the Git profile. The original installer is unchanged.
3. The next dump confirms a C06D007F delay-load failure for
   `user32.dll!GetWindowDpiAwarenessContext`. The body's USER32 delay-import
   name at RVA 0x3c97d6 is redirected to existing KxUser adapters.

| Image | Timestamp | Size | Entry RVA | Patch RVA | Flag RVA |
| --- | --- | --- | --- | --- | --- |
| outer .exe | 698c6aab | f6000 | b1e60 | 5734 | b7060 |
| extracted .tmp | 698c6aae | 44b000 | 3ab668 | 7438 | 3b8064 |

Runtime patches require the inspected basename, PE identity and original bytes,
real NT 6.0, and application-specific workarounds enabled. No Git-specific
common-control thunk hook is applied to Sublime.

## Verification and deployment

- `audit/sublime-final.log`: normal EXE launch; OS version 10.0.19045.
- `audit/sublime-final-windows.txt`: KexDll loaded; wizard/control responses.
- `audit/sublime-config-test.txt`: disable/enable exit 0; debugger removed then
  restored in the WOW64 IFEO registry view.
- `audit/sublime-final-regression.txt`: parent and child KexDll loaded,
  propagation status 0, version APIs 10.0.19045, child exit 0.
- Ghidra project contains the original outer installer and a reconstructed
  body from the AVX dump. The latter is a memory reconstruction, not an original
  signed on-disk executable.

Final build and downloaded-back deployed files have matching SHA256:

| File | SHA256 |
| --- | --- |
| x86 KexDll.dll | 340e5398e41863e4db581aa632077f47da804cb5382277874aa2b018e0b58da5 |
| x64 KexCfg.exe | f03947ebd11a112e8f64caba7bcb4878f4b7417159d2aeb0cae71fdd496119ac |
| x64 KexShlEx.dll | 7b9007157c2de0b2e4da30cbee5c191b15768318e5ce3c13b4c47489b4632e09 |

The VM runtime/configuration/shell binaries and Installer staging files are
updated. Previous binaries are retained under pre-sublime names. Explorer
was not restarted: an already loaded shell extension can retain the old code
until Explorer is restarted or the user signs in again. The current installer
configuration was applied and tested with the new KexCfg directly. Visual
rendering has not been certified. No commit or release was made for this fix.
