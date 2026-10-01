# VXL reader validation (2026-10-01)

The current NEXT 1.2.3.2463 source also contains the single-read boundary and
range-read loop defects found during viewer integration. Vista now rejects an
index equal to the event count, reads every entry in an inclusive range, clips
the end to the last entry, rejects write-mode handles, and rejects null output
entry pointers. This includes a one-entry range at the end of a log.

Before publishing a read index, VxlpBuildIndex checks the physical file size,
64-bit sum of event counts, fixed source-string termination, each entry's
extent, severity and source indices, UTF-16 length representation and required
terminators. It also checks the observed severity totals against the header.
Invalid files return STATUS_FILE_INVALID with no surviving read handle.
Files above the existing 32-bit file-offset format limit are rejected.

Isolated source builds:

```powershell
powershell -ExecutionPolicy Bypass -File build_kexdll.ps1 -OutputDirectory audit/build-vxl-reader/x64
powershell -ExecutionPolicy Bypass -File build_kexdll_x86.ps1 -OutputDirectory audit/build-vxl-reader/x86
powershell -ExecutionPolicy Bypass -File tests/build_vxl_reader_probe.ps1 -Architecture x64
powershell -ExecutionPolicy Bypass -File tests/build_vxl_reader_probe.ps1 -Architecture x86
```

The native and WOW64 probes ran on the disposable Server 2008 clone with their
matching new KexDll beside the executable; no installed VxKex or global settings
were required. Each creates an exclusively new `viewer-reader-fixture-<arch>.vxl`
and reads all six severities and Japanese text. They test write-mode rejection,
one-past-end, an oversized clipped inclusive range, a single final item and a
null destination. Ten owned malformed copies cover truncated header, truncated
final entry, overflowing event counts, invalid severity/component/file indices,
missing text-header terminator, oversized body length and unterminated
application/function strings. Each copy is CREATE_NEW and removed after closing.

Evidence: `audit/vxl-reader-result-x64.txt` and
`audit/vxl-reader-result-x86.txt` both have Failures=0. All ten malformed cases
return c0000098 with a null handle. These probes validate the reader, not the
remaining viewer dialogs, export-thread failure handling or complete GUI flow.
The initial TLS candidate remains unchanged and does not contain this later
reader fix. Installer and the original VM were not modified.
