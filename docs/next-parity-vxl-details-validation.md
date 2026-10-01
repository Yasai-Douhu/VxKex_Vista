# Viewer details and raw navigation (2026-10-01)

The command-driven details probe creates its own window station and desktop,
starts only its owned viewer there, and never switches the visible desktop.
It retains the search/case/wildcard/inversion/Unicode-body/severity checks from
the earlier GUI probe, then tests the actual raw-item dialog and details panel.

Two production defects were corrected:

- Raw navigation accepted zero as the first item; it now requires a positive
  successfully parsed one-based item number before converting to a raw index.
- GetLogEntryRawIndex accessed the filtered lookup array before checking the
  display index. It now rejects absent/out-of-range cache access. Raw-to-display
  lookup also rejects raw indices outside the opened file.

Both x64 and x86 viewers reject 0 and 7 in a six-entry file, navigate to item 6,
and show `record 5`, `日本語 5`, `fixture.c`, source line 105, `WriteFixture`,
severity and date/time. Item 3 is rejected while warning entries are hidden and
accepted after restoring that filter. The dialogs close, and each owned viewer
exits with code zero. Evidence: `audit/vxl-details-x64.txt` and x86 both end in
`Exit=0 Failures=0`.

The first cross-process GetDlgItemText read of the multiline edit returned an
empty result although WM_GETTEXTLENGTH and EM_GETLINE proved its internal text
was populated. The probe now reads actual edit lines with EM_GETLINE and verifies
the assembled Unicode text; no product workaround was added for that diagnostic
artifact. Initial diagnostic reports remain under
`audit/vxl-details-diagnostic-<arch>.txt`. One initial x64 build used a stale
configuration archive and failed association linking; rebuilding with the current
helper library resolved it. Neither case was treated as runtime success.

Build the current viewer with the current helper archive, then the probe:

```powershell
powershell -ExecutionPolicy Bypass -File build_vxlview.ps1 -Architecture x64 -OutputDirectory audit/build-vxl-details/x64 -ConfigurationLibrary audit/build-global-settings/KxCfgHlp/KxCfgHlp.lib
powershell -ExecutionPolicy Bypass -File build_vxlview.ps1 -Architecture x86 -OutputDirectory audit/build-vxl-details/x86 -ConfigurationLibrary audit/build-gui-reader-x86/KxCfgHlp/KxCfgHlp.lib
powershell -ExecutionPolicy Bypass -File tests/build_vxlview_details_probe.ps1 -Architecture x64
powershell -ExecutionPolicy Bypass -File tests/build_vxlview_details_probe.ps1 -Architecture x86
```

The probe takes viewer image, log file, and output report as three arguments.
The clone used the matching new reader DLL in VxlReader-x64/x86 beside the new
viewer and probe, and the previously generated reader fixtures. Installer,
TLS candidate and the original VM are unchanged. Export dialogs/thread failures,
remaining high-priority GUI/setup/TLS integration and final packaging remain open.
