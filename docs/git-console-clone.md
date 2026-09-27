# Git HTTPS clone with a Vista console

On Server 2008 x64, Git 2.55.0.windows.5 failed with
`fatal: remote helper 'https' aborted session` when stderr was attached to a
console. Redirecting stderr to a file made the same clone succeed.

## Cause and repair

The failing remote helper had both native standard input and output set to
INVALID_HANDLE_VALUE, while stderr retained a legacy console handle. MSVCRT
initialized stdin with file descriptor -2; getc returned EOF with errno 9
(EBADF). This happened before HTTPS requests were sent.

Git passes the standard handles through PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
NT 6.0 console handles are CSRSS handles, not kernel handles. A mixed list of
pipe and legacy console handles is accepted by native UpdateProcThreadAttribute
but does not correctly inherit the pipe. A standalone native test reproduces
the issue without Git (child GetFileType(stdout) returns FILE_TYPE_UNKNOWN).

KxBase now rejects a HANDLE_LIST containing a valid legacy console handle on
real NT 6.0 with ERROR_INVALID_PARAMETER before the attribute list is modified.
Git consequently takes its existing CreateProcess fallback. The shim does not
silently discard a restriction or modify the caller's input array. Applications
without a fallback will receive the unsupported-parameter error; this is not a
general implementation of modern restricted console-handle inheritance.

The check supports write-only console handles (GetConsoleMode alone would reject
them), preserves kernel-only lists, and excludes INVALID_HANDLE_VALUE.

## Validation

- PID 1948's console, normal stdout/stderr: failure before the change.
- stdout redirected only: failure; stderr redirected only: success.
- Native mixed-list probe: update=1, child pipe invalid, child exit=1.
- Compatibility mixed-list probe: update=0, error=87.
- Compatibility pipe-only probe: update=1, error=0.
- Normal console clone with the repair: exit=0.
- Final clone at C:\VxKexProbe\GitClone-Console-trace-7848254: full clone,
  HEAD a9bdf224e8bdacc75e93e705da623d0ad5e3e25b; fsck --full and status
  exit 0 with no output. See audit/git-console-final-clean-trace.json and
  audit/git-console-verify-trace.json. This run used restored production KxCrt.
- Deployed System32 KxBase and Installer/KxBase SHA256:
  44618BC312FAE2A46776DCF011A4B592C428F5FC3A03E1C3E085A3E8EDEDE770.
- Temporary CRT/read tracing is removed from source and the VM's CRT DLL is
  restored. Original VM DLL backups retain the `pre-git-console` names.

The focused probe is tests/vista_handle_list_probe.c. Enable VxKex for that EXE
before running; its native branch resolves kernel32 through the module snapshot
to bypass rewritten imports. The diagnostic currently attaches to console PID
1948. Tested runtime is x64; the shared-source x86 change is not deployed here.

References:
- https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute
- https://github.com/git-for-windows/git/blob/main/compat/mingw.c
- https://github.com/rprichard/win32-console-docs (legacy console inheritance experiments)
