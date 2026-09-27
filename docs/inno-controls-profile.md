# Automatic Inno Setup controls profile (2026-09-27)

The x86 runtime on real NT 6.0 now recognizes the Inno Setup attribution
`This installation was built with Inno Setup.` in the image resource directory.
It locates COMCTL32's InitCommonControls import by name and scans executable
sections for its indirect jump thunks. The matched thunks invoke the existing
standard-control registration helper after the original initialization.
The helper uses the IAT RVA discovered for this process. Fixed Git/Sublime
control-thunk offsets have been removed.

The profile runs under the existing DisableAppSpecific gate. It requires
VxKex enabled/inherited, a matching resource attribution, a normal named import
with OriginalFirstThunk available, and x86 `ff 25` thunks. It does not claim
coverage of every Inno version, native x64 setup engines, modified resources,
or alternate initialization paths. No new checkbox is needed for these tested
cases. AVX and DPI delay-load fixes remain separately limited to inspected
Git/Sublime builds; they are not generalized by this drawing change.

## VM verification

VMware framebuffer screenshots show labels, buttons and content drawn for:

- VS Code 1.70.2: `audit/vscode-inno-auto-final.png` (license page).
- Sublime Text 4213: `audit/sublime-inno-auto.png` (destination page).
- Git 2.55.0.5: `audit/git-inno-auto.png` (information page).

These confirm initial-page drawing, not complete installation or Aero-themed
appearance of every control. No installer was advanced to installation.
The WOW64 parent/child regression passed (`audit/inno-auto-regression.txt`).
Build and deployed SysWOW64 DLL match SHA256
`2da83ddd39a22f399068da6ff030cc8a08724e4c9ffe53684609797e49e63d56`.
The VM Kex32 runtime and Installer/Kex32 staging DLL are also updated.

## Missing property tab

Registration remained intact. The preceding shell rebuild had incorrectly
linked registry functions to KERNEL32 using the project's newer import library;
LoadLibrary failed with ERROR_PROC_NOT_FOUND (127). Relinking with SDK kernel32
and user32 import libraries resolves those registry imports through ADVAPI32.
`build_kexshlex.ps1` now uses the SDK libraries. The repaired DLL loads with
error 0 and exports DllGetClassObject. The VxKex tab is visible again in
`audit/shell-tab-restored2.png`. The VM and Installer/KexShlEx.dll are updated,
and the previous DLL is retained as KexShlEx.pre-importfix.dll in the VM.
