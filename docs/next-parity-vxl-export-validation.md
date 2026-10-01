# Export failure recovery (2026-10-01)

ExportLog now checks the allocation of its owned destination name before copying
it. Allocation failure reports an error and leaves the owner usable. This fixes
a null-pointer crash in the export-start path.

The diagnostic executable links the actual current viewer objects and the viewer
resource/manifest, with the fixed reader DLL beside it. It creates an owned
window station/desktop that is never displayed. Fault wrappers change only the
diagnostic executable's imports, and are restored at the end:

- One RtlAllocateHeap call fails, exercising the new allocation guard.
- RtlCreateUserThread fails with STATUS_NO_MEMORY, exercising real start cleanup.
- Successful thread creation invokes the actual OS function with a suspended
  thread, allowing the caller's stack filename to be overwritten before resume.
  The real worker still exports to its copied destination.
- TaskDialogIndirect is intercepted to record notification text and avoid
  requiring user interaction; rendering of those notifications is not covered.

Actual worker/file operations cover a missing directory (c000003a), exclusively
locked output (c0000043), and successful UTF-16 export with BOM, endpoint log
entries and Japanese body. The probe pumps UI messages, waits for actual worker
termination, checks the exact exit status, and requires the owner and arrow
cursor to recover. The opened log remains readable after each operation.
Output fixtures are CREATE_NEW or absent-before and removed by the probe.

Both Server 2008 native x64 and WOW64 x86 runs passed:
`audit/vxl-export-failure-x64.txt` and x86 end in `Failures=0`.

Build with `tests/build_vxlview_export_failure_probe.ps1`, specifying architecture,
OutputDirectory, ViewerBuildDirectory and the current ConfigurationLibrary.
The script links only objects corresponding to viewer source files, excluding
unrelated diagnostic objects. It includes VxlView.res for Common Controls v6.
The initial diagnostic build omitted that manifest and could not load the
required COMCTL32 ordinal; the diagnostic import resolver was also extended for
ordinal imports. Those were probe assembly issues, not product runtime failures.

The native save-file dialog interaction, notification rendering and additional
resource-exhaustion stress are TODOs. Representative normal export and principal
failure recovery now have evidence in both architectures; these TODOs do not
prevent the main export feature from functioning. Installer integration remains
the next required phase gate.
