# PostgreSQL 18 on Windows Vista / Server 2008

PostgreSQL 18.6 failed during `initdb` with a missing
`KERNEL32!GetSystemTimePreciseAsFileTime` import. VxKex already implements that
API, but the PostgreSQL executables had not all been enabled in VxKex. After
enabling them, `initdb` still reported `invalid binary` and Win32 error 127.

The second failure came from PostgreSQL's Windows file-opening code. Its
`initialize_ntdll()` resolves `NtFlushBuffersFileEx` before opening any file;
that export is absent from Server 2008's `ntdll.dll`. Consequently,
`find_my_exec()` could not validate even its own executable. VxKex now exports
`NtFlushBuffersFileEx` from `KxNt.dll` using Vista's older
`NtFlushBuffersFile` system call. This performs a full flush, which is stronger
than PostgreSQL's requested data-only flush and can cost more I/O time.

The compatibility layer must be enabled for **every PostgreSQL executable that
can start directly or as a child**, particularly `initdb.exe`, `postgres.exe`,
`pg_ctl.exe`, and `psql.exe`. Apply VxKex to the installation's `bin\*.exe`,
then start fresh processes so the new DLL is loaded.

## Server 2008 validation

- PostgreSQL 18.6 `initdb` completed in a temporary cluster and in
  `C:\Program Files\PostgreSQL\18\data`.
- The production cluster uses `scram-sha-256` authentication.
- `postgresql-x64-18` is an automatic Windows service running as
  `NT AUTHORITY\LocalService` with access to the data directory.
- After stopping and restarting the service, `psql` returned
  `postgres|18.6|C:/Program Files/PostgreSQL/18/data`.

The original EDB installer had left no initialized cluster or service. The
production cluster and service above were created after applying the fix; the
installer itself was not rerun.

Sources: [PostgreSQL `initialize_ntdll()`](https://github.com/postgres/postgres/blob/REL_18_STABLE/src/port/win32ntdll.c),
[PostgreSQL Windows file opening](https://github.com/postgres/postgres/blob/REL_18_STABLE/src/port/open.c),
[Microsoft `NtFlushBuffersFileEx` reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntflushbuffersfileex).
