# Inno Setup IPropertyStore::Commit on Vista (2026-09-29)

## Reproduction and cause

The reported VS Code Insiders 1.140.0 installer fails while creating shortcuts:
`IPropertyStore::Commit`, `0x80070057` (`E_INVALIDARG`). This is separate from
the earlier ShellLink CoCreateInstance/DLL-search-policy issue.

On the Vista x64 VM, a native x86 ShellLink probe reproduces E_INVALIDARG from
Commit even with an empty store. SetValue accepts each Inno AppUserModel key
(IDs 5, 8, 9, 12, 26), but Commit fails. Native IPersistFile::Save succeeds;
reloading the link preserves its target but does not return those properties.
These results do not support claiming that ignoring Commit preserves metadata.

References:

- [Inno shortcut implementation](https://github.com/jrsoftware/issrc/blob/main/Projects/Src/Setup.InstFunc.Ole.pas)
- [Microsoft AppUserModel.ID documentation](https://learn.microsoft.com/en-us/windows/win32/properties/props-system-appusermodel-id)

## Compatibility behavior

KxCom configures the native ShellLink property-store Commit slot after successful
CoCreateInstance/CoCreateInstanceEx, only for identified Inno Setup processes on
real NT 6.0 with application-specific workarounds enabled. Installation of the
hook is serialized. Native object identity, lifetime and other methods remain.

The fallback is limited to native E_INVALIDARG, a store that also exposes
IShellLinkW, and a nonempty set consisting exclusively of the five recognized
AppUserModel keys with their expected value types. It treats those optional
Win7+ taskbar/Win10 toast metadata properties as unsupported on Vista. It does
not persist them. Unknown keys, an empty store, other errors, disabled profiles
and unrelated COM classes keep the native result. Native IPersistFile::Save
still performs actual shortcut creation and reports filesystem errors.

The existing Inno detector targets x86 engines; this deployment updates only
the x86 KxCom binary. It is not a blanket change to Windows property stores.

## Verification

`tests/vista_property_store_probe.cpp` was compiled with VS2010 /MT and the
synthetic Inno resource in `tests/vista_shelllink_probe.rc`. Run with `<log>
compat` for the enabled profile, or just `<log>` for native/disabled behavior.
Each enabled/disabled test runs in its own process.

- Enabled and disabled tests both report `Failures=0`.
- Both CoCreateInstance and CoCreateInstanceEx paths are exercised.
- Each recognized key gets S_OK only with the profile enabled.
- Empty stores and unknown property ID 999 retain E_INVALIDARG.
- Saved links reload with the original notepad target.
- Saving to a nonexistent directory retains error `80070003`.

The actual installer at
`C:\Users\Vista\Desktop\検証用インストーラ\VSCodeUserSetup-x64-1.140.0-insider.exe`
was then run as Vista with `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=...`.
The log records `Successfully created the icon.` for both Start Menu and
desktop shortcuts, followed by `Installation process succeeded.`
The resulting Start Menu .lnk was read back through WScript.Shell: it exists,
targets the existing `C:\Users\Vista\AppData\Local\Programs\Microsoft VS Code Insiders\Code - Insiders.exe`,
and has the corresponding working directory. No UI clicking was used.

Local evidence: `audit/property-native.txt`, `property-enabled.txt`,
`property-disabled.txt`, `property-shortcut.txt`, `insiders-shortcut-fixed.log`.

## Deployment

Updated Vista VM `C:\Windows\SysWOW64\KxCom.dll`,
`C:\VxKex\Kex32\KxCom.dll`, and repository `Installer/Kex32/KxCom.dll`.
The guest originals are backed up alongside them as
`KxCom.pre-property-20260929.dll`. Retrieved guest copies match the package:

```
SHA256 0CB3268FF2393B646AEF7E3A1E981854E772D110FF34782168D8EBEF72252745
```

Restart an already-running installer to load the new library. This task did
not publish a release or commit the changes.
