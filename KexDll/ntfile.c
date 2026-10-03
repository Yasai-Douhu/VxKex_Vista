///////////////////////////////////////////////////////////////////////////////
// Extended file information classes mapped to NT 6.0 file operations.
// Unlike NEXT, unsupported flags are never silently discarded.
///////////////////////////////////////////////////////////////////////////////
#include "buildcfg.h"
#include "kexdllp.h"

typedef struct _KEX_FILE_RENAME_INFORMATION {
	ULONG Flags; // BOOLEAN ReplaceIfExists in the legacy layout; padding is zero.
	HANDLE RootDirectory;
	ULONG FileNameLength;
	WCHAR FileName[1];
} KEX_FILE_RENAME_INFORMATION;

static NTSTATUS KexSetExtendedFileInformation(
	HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS InformationClass)
{
	NTSTATUS Status;
	if ((ULONG) InformationClass == 64) { // FileDispositionInformationEx
		ULONG Flags;
		BOOLEAN DeleteFile;
		if (Length != sizeof(Flags)) {
			return STATUS_INFO_LENGTH_MISMATCH;
		}
		try {
			Flags = *(PULONG) FileInformation;
		} except (EXCEPTION_EXECUTE_HANDLER) {
			return GetExceptionCode();
		}
		if (Flags & ~1UL) {
			return STATUS_NOT_SUPPORTED;
		}
		DeleteFile = (BOOLEAN) (Flags & 1);
		return NtSetInformationFile(FileHandle, IoStatusBlock, &DeleteFile,
			sizeof(DeleteFile), FileDispositionInformation);
	} else if ((ULONG) InformationClass == 65) { // FileRenameInformationEx
		KEX_FILE_RENAME_INFORMATION Header;
		KEX_FILE_RENAME_INFORMATION *Captured;
		const ULONG HeaderLength = FIELD_OFFSET(KEX_FILE_RENAME_INFORMATION, FileName);
		ULONG CaptureLength;
		if (Length < sizeof(Header)) {
			return STATUS_INFO_LENGTH_MISMATCH;
		}
		try {
			RtlCopyMemory(&Header, FileInformation, HeaderLength);
		} except (EXCEPTION_EXECUTE_HANDLER) {
			return GetExceptionCode();
		}
		if (Header.Flags & ~1UL) {
			return STATUS_NOT_SUPPORTED;
		}
		// Subtraction avoids wrapping HeaderLength + a caller-supplied length.
		if (!Header.FileNameLength || (Header.FileNameLength & 1) ||
			Header.FileNameLength > Length - HeaderLength) {
			return STATUS_INVALID_PARAMETER;
		}
		// NT object paths are UNICODE_STRINGs. Bound allocation independently
		// of the caller's buffer size; trailing bytes are never copied.
		if (Header.FileNameLength > 65534) {
			return STATUS_NAME_TOO_LONG;
		}
		CaptureLength = HeaderLength + Header.FileNameLength;
		if (CaptureLength < sizeof(Header)) {
			CaptureLength = sizeof(Header);
		}
		Captured = (KEX_FILE_RENAME_INFORMATION *) HeapAlloc(
			GetProcessHeap(), HEAP_ZERO_MEMORY, CaptureLength);
		if (!Captured) {
			return STATUS_NO_MEMORY;
		}
		Captured->Flags = Header.Flags & 1;
		Captured->RootDirectory = Header.RootDirectory;
		Captured->FileNameLength = Header.FileNameLength;
		try {
			RtlCopyMemory(Captured->FileName,
				(PBYTE) FileInformation + HeaderLength, Header.FileNameLength);
			Status = NtSetInformationFile(FileHandle, IoStatusBlock, Captured,
				CaptureLength, FileRenameInformation);
		} except (EXCEPTION_EXECUTE_HANDLER) {
			Status = GetExceptionCode();
		}
		HeapFree(GetProcessHeap(), 0, Captured);
		return Status;
	}
	return NtSetInformationFile(FileHandle, IoStatusBlock, FileInformation,
		Length, InformationClass);
}

KEXAPI NTSTATUS NTAPI Ext_NtSetInformationFile(
	HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation, ULONG FileInformationLength,
	FILE_INFORMATION_CLASS FileInformationClass)
{
	ULONG SavedError = GetLastError();
	NTSTATUS Status = KexSetExtendedFileInformation(FileHandle, IoStatusBlock,
		FileInformation, FileInformationLength, FileInformationClass);
	SetLastError(SavedError);
	return Status;
}
