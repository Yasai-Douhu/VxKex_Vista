///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     vxlpriv.c
//
// Abstract:
//
//     Contains miscellaneous private routines.
//
// Author:
//
//     vxiiduu (30-Sep-2022)
//
// Revision History:
//
//     vxiiduu	            30-Sep-2022  Initial creation.
//     vxiiduu              15-Oct-2022  Convert to v2 format.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kexdllp.h"

#pragma warning(disable:4244)

NTSTATUS VxlpFlushLogFileHeader(
	IN	VXLHANDLE			LogHandle)
{
	PVOID Base;
	SIZE_T Size;
	IO_STATUS_BLOCK IoStatusBlock;

	ASSERT (LogHandle != NULL);
	ASSERT (LogHandle->Header != NULL);
	ASSERT (LogHandle->OpenMode == GENERIC_WRITE);

	Base = LogHandle->Header;
	Size = sizeof(*LogHandle->Header);

	return NtFlushVirtualMemory(
		NtCurrentProcess(),
		&Base,
		&Size,
		&IoStatusBlock);
}

ULONG VxlpGetTotalLogEntryCount(
	IN	VXLHANDLE			LogHandle)
{
	ULONG Index;
	ULONG Total;

	ASSERT (LogHandle != NULL);
	ASSERT (LogHandle->Header != NULL);

	Total = 0;

	ForEachArrayItem (LogHandle->Header->EventSeverityTypeCount, Index) {
		Total += LogHandle->Header->EventSeverityTypeCount[Index];
	}

	return Total;
}

ULONG VxlpSizeOfLogFileEntry(
	IN	PVXLLOGFILEENTRY	Entry)
{
	ULONG Size;

	ASSERT (Entry != NULL);

	Size = sizeof(VXLLOGFILEENTRY);
	Size += Entry->TextHeaderCch * sizeof(WCHAR);
	Size += Entry->TextCch * sizeof(WCHAR);

	return Size;
}

NTSTATUS VxlpBuildIndex(
	IN	VXLHANDLE			LogHandle)
{
	PVXLLOGFILEENTRY Entry;
	ULONG TotalLogEntryCount;
	ULONG Index;
	ULONG SeverityIndex[LogSeverityMaximumValue];
	FILE_STANDARD_INFORMATION FileInfo;
	IO_STATUS_BLOCK IoStatus;
	NTSTATUS Status;
	ULONGLONG Total, Offset, Size;
	ULONG Item, Character;

	ASSERT (LogHandle != NULL);
	ASSERT (LogHandle->OpenMode == GENERIC_READ);
	ASSERT (LogHandle->EntryIndexToFileOffset == NULL);

	Status = NtQueryInformationFile(LogHandle->FileHandle, &IoStatus, &FileInfo,
		sizeof(FileInfo), FileStandardInformation);
	if (!NT_SUCCESS(Status)) return Status;
	if (FileInfo.EndOfFile.QuadPart < sizeof(VXLLOGFILEHEADER) ||
		FileInfo.EndOfFile.QuadPart > 0xffffffffUL) return STATUS_FILE_INVALID;
	// Fixed-size source strings are consumed as C strings by the viewer.
#define CHECK_STRINGS(Member) \
	for (Item = 0; Item < ARRAYSIZE(LogHandle->Header->Member); ++Item) { \
		for (Character = 0; Character < ARRAYSIZE(LogHandle->Header->Member[0]); ++Character) \
			if (!LogHandle->Header->Member[Item][Character]) break; \
		if (Character == ARRAYSIZE(LogHandle->Header->Member[0])) return STATUS_FILE_INVALID; \
	}
	CHECK_STRINGS(SourceComponents);
	CHECK_STRINGS(SourceFiles);
	CHECK_STRINGS(SourceFunctions);
#undef CHECK_STRINGS
	for (Character = 0; Character < ARRAYSIZE(LogHandle->Header->SourceApplication); ++Character)
		if (!LogHandle->Header->SourceApplication[Character]) break;
	if (Character == ARRAYSIZE(LogHandle->Header->SourceApplication)) return STATUS_FILE_INVALID;
	Total = 0;
	for (Index = 0; Index < LogSeverityMaximumValue; ++Index)
		Total += LogHandle->Header->EventSeverityTypeCount[Index];
	if (Total > (FileInfo.EndOfFile.QuadPart - sizeof(VXLLOGFILEHEADER)) / sizeof(VXLLOGFILEENTRY))
		return STATUS_FILE_INVALID;
	TotalLogEntryCount = (ULONG) Total;

	if (!TotalLogEntryCount) {
		return STATUS_NO_MORE_ENTRIES;
	}

	//
	// Allocate memory for the index.
	//

	LogHandle->EntryIndexToFileOffset = SafeAllocSeh(ULONG, TotalLogEntryCount);

	Offset = sizeof(VXLLOGFILEHEADER);
	RtlZeroMemory(SeverityIndex, sizeof(SeverityIndex));

	for (Index = 0; TotalLogEntryCount--; ++Index) {
		if (Offset + sizeof(VXLLOGFILEENTRY) > (ULONGLONG) FileInfo.EndOfFile.QuadPart)
			return STATUS_FILE_INVALID;
		Entry = (PVXLLOGFILEENTRY) (LogHandle->MappedFile + (ULONG) Offset);
		Size = VxlpSizeOfLogFileEntry(Entry);
		if (Offset + Size > (ULONGLONG) FileInfo.EndOfFile.QuadPart ||
			Entry->Severity >= LogSeverityMaximumValue || Entry->Severity < 0 ||
			Entry->SourceComponentIndex >= ARRAYSIZE(LogHandle->Header->SourceComponents) ||
			Entry->SourceFileIndex >= ARRAYSIZE(LogHandle->Header->SourceFiles) ||
			!Entry->TextHeaderCch || Entry->TextHeaderCch > 0xffffUL / sizeof(WCHAR) ||
			Entry->TextCch > 0xffffUL / sizeof(WCHAR)) return STATUS_FILE_INVALID;
		if (Entry->Text[Entry->TextHeaderCch - 1] ||
			(Entry->TextCch && Entry->Text[Entry->TextHeaderCch + Entry->TextCch - 1]))
			return STATUS_FILE_INVALID;
		++SeverityIndex[Entry->Severity];
		//
		// record file offset of the Index'th entry into the index,
		// for fast seeking to any particular log entry
		//

		LogHandle->EntryIndexToFileOffset[Index] = (ULONG) VA_TO_RVA(LogHandle->MappedFile, Entry);

		//
		// skip ahead to next entry
		//

		Offset += Size;
	}
	for (Index = 0; Index < LogSeverityMaximumValue; ++Index)
		if (SeverityIndex[Index] != LogHandle->Header->EventSeverityTypeCount[Index]) return STATUS_FILE_INVALID;

	return STATUS_SUCCESS;
}

NTSTATUS VxlpFindOrCreateSourceComponentIndex(
	IN	VXLHANDLE			LogHandle,
	IN	PCWSTR				SourceComponent,
	OUT	PUCHAR				SourceComponentIndex)
{
	ULONG Index;

	ASSERT (LogHandle != NULL);
	ASSERT (SourceComponent != NULL);
	ASSERT (SourceComponentIndex != NULL);
	ASSERT (wcslen(SourceComponent) < ARRAYSIZE(LogHandle->Header->SourceComponents[0]));

	for (Index = 0; Index < ARRAYSIZE(LogHandle->Header->SourceComponents); ++Index) {
		if (StringEqual(LogHandle->Header->SourceComponents[Index], SourceComponent)) {
			*SourceComponentIndex = Index;
			return STATUS_SUCCESS;
		} else if (LogHandle->Header->SourceComponents[Index][0] == '\0') {
			HRESULT Result;

			Result = StringCchCopy(
				LogHandle->Header->SourceComponents[Index],
				ARRAYSIZE(LogHandle->Header->SourceComponents[Index]),
				SourceComponent);

			if (FAILED(Result)) {
				return STATUS_BUFFER_TOO_SMALL;
			}

			*SourceComponentIndex = Index;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_TOO_MANY_INDICES;
}

NTSTATUS VxlpFindOrCreateSourceFileIndex(
	IN	VXLHANDLE			LogHandle,
	IN	PCWSTR				SourceFile,
	OUT	PUCHAR				SourceFileIndex)
{
	ULONG Index;

	ASSERT (LogHandle != NULL);
	ASSERT (SourceFile != NULL);
	ASSERT (SourceFileIndex != NULL);
	ASSERT (wcslen(SourceFile) < ARRAYSIZE(LogHandle->Header->SourceFiles[0]));

	for (Index = 0; Index < ARRAYSIZE(LogHandle->Header->SourceFiles); ++Index) {
		if (StringEqual(LogHandle->Header->SourceFiles[Index], SourceFile)) {
			*SourceFileIndex = Index;
			return STATUS_SUCCESS;
		} else if (LogHandle->Header->SourceFiles[Index][0] == '\0') {
			HRESULT Result;

			Result = StringCchCopy(
				LogHandle->Header->SourceFiles[Index],
				ARRAYSIZE(LogHandle->Header->SourceFiles[Index]),
				SourceFile);

			if (FAILED(Result)) {
				return STATUS_BUFFER_TOO_SMALL;
			}

			*SourceFileIndex = Index;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_TOO_MANY_INDICES;
}

NTSTATUS VxlpFindOrCreateSourceFunctionIndex(
	IN	VXLHANDLE			LogHandle,
	IN	PCWSTR				SourceFunction,
	OUT	PUCHAR				SourceFunctionIndex)
{
	ULONG Index;

	ASSERT (LogHandle != NULL);
	ASSERT (SourceFunction != NULL);
	ASSERT (SourceFunctionIndex != NULL);
	ASSERT (wcslen(SourceFunction) < ARRAYSIZE(LogHandle->Header->SourceFunctions[0]));

	for (Index = 0; Index < ARRAYSIZE(LogHandle->Header->SourceFunctions); ++Index) {
		if (StringEqual(LogHandle->Header->SourceFunctions[Index], SourceFunction)) {
			*SourceFunctionIndex = Index;
			return STATUS_SUCCESS;
		} else if (LogHandle->Header->SourceFunctions[Index][0] == '\0') {
			HRESULT Result;

			Result = StringCchCopy(
				LogHandle->Header->SourceFunctions[Index],
				ARRAYSIZE(LogHandle->Header->SourceFunctions[Index]),
				SourceFunction);

			if (FAILED(Result)) {
				return STATUS_BUFFER_TOO_SMALL;
			}

			*SourceFunctionIndex = Index;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_TOO_MANY_INDICES;
}
