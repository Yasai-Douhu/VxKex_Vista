# Markdown webview: confirmed Vista directory enumeration incompatibility

Validation date: 2026-09-20
Environment: Windows Server 2008 NT 6.0, VS Code 1.138.0 at C:\vscode\Code.exe.

Both the old and a fresh profile logged Service Worker Database IO errors.
Chromium's directory enumerator requests FindExInfoBasic together with
FIND_FIRST_EX_LARGE_FETCH. These options require Windows 7 / Server 2008 R2;
the previous KxBase exports forwarded them unchanged to Vista's kernel32.

The fix translates FindExInfoBasic to FindExInfoStandard and removes only
FIND_FIRST_EX_LARGE_FETCH on the actual NT 6.0 OS. Other flags and search
parameters are retained. Both ANSI and Unicode entry points are implemented.
No ConPTY changes are included.

## User-run probe results

| Call | Result | Entries | Last error |
| --- | --- | --- | --- |
| Native standard | Success | 91 | 18 (end of enumeration) |
| Native Basic + LargeFetch | Failure | 0 | 87 (invalid parameter) |
| Fixed standard | Success | 91 | 18 |
| Fixed Basic | Success | 91 | 18 |
| Fixed LargeFetch | Success | 91 | 18 |
| Fixed Basic + LargeFetch | Success | 91 | 18 |
| Fixed unknown flag 0x80000000 | Failure | 0 | 87 |

The user confirmed Markdown preview renders successfully after applying the
candidate and supplied a screenshot. The Unicode enumeration path is therefore
verified on the VM. The ANSI export was built but not independently exercised.
The old profile's recovery after prior storage failures remains unverified.

Raw evidence is retained locally in audit/webview/probe-result.txt,
install-result.txt, and the pre-fix run-*.log files. The candidate is installed
in the VM; original DLLs were backed up by Apply-Webview-Fix.cmd.

References:
- https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfileexw
- https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ne-minwinbase-findex_info_levels
- https://github.com/chromium/chromium/blob/main/base/files/file_enumerator_win.cc
