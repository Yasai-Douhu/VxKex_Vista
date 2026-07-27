#include "buildcfg.h"
#include <KexSmp.h>

SMPAPI NTSTATUS NTAPI SmpCreateStringMapper(
	OUT		PPKEX_SMP_STRING_MAPPER		StringMapper,
	IN		ULONG						Flags OPTIONAL)
{
	PKEX_SMP_STRING_MAPPER Mapper;

	if (!StringMapper) {
		return STATUS_INVALID_PARAMETER_1;
	}

	*StringMapper = NULL;

	if (Flags & ~(KEX_SMP_STRING_MAPPER_FLAGS_VALID_MASK)) {
		return STATUS_INVALID_PARAMETER_2;
	}

	Mapper = SafeAlloc(KEX_SMP_STRING_MAPPER, 1);
	if (!Mapper) {
		return STATUS_NO_MEMORY;
	}
	RtlZeroMemory(Mapper, sizeof(*Mapper));

	Mapper->Flags = Flags;
	*StringMapper = Mapper;

	return STATUS_SUCCESS;
}

SMPAPI NTSTATUS NTAPI SmpDeleteStringMapper(
	IN		PPKEX_SMP_STRING_MAPPER		StringMapper)
{
	PKEX_SMP_STRING_MAPPER Mapper;
	ULONG i;

	if (!StringMapper || !*StringMapper) {
		return STATUS_INVALID_PARAMETER_1;
	}

	Mapper = *StringMapper;

	for (i = 0; i < KEX_SMP_HASH_BUCKETS; i++) {
		PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Entry = Mapper->Buckets[i];
		while (Entry) {
			PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Next = Entry->Next;
			SafeFree(Entry);
			Entry = Next;
		}
	}

	SafeFree(*StringMapper);

	return STATUS_SUCCESS;
}

SMPAPI NTSTATUS NTAPI SmpInsertEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER		StringMapper,
	IN		PCUNICODE_STRING			Key,
	IN		PCUNICODE_STRING			Value)
{
	PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Entry;
	ULONG KeySignature;
	ULONG BucketIndex;

	if (!StringMapper) {
		return STATUS_INVALID_PARAMETER_1;
	}

	if (!Key) {
		return STATUS_INVALID_PARAMETER_2;
	}

	if (!Value) {
		return STATUS_INVALID_PARAMETER_3;
	}

	Entry = SafeAlloc(KEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY, 1);
	if (!Entry) {
		return STATUS_NO_MEMORY;
	}

	RtlHashUnicodeString(
		Key,
		(StringMapper->Flags & KEX_SMP_STRING_MAPPER_CASE_INSENSITIVE_KEYS) != 0,
		HASH_STRING_ALGORITHM_DEFAULT,
		&KeySignature);

	Entry->Hash = KeySignature;
	Entry->Key = *Key;
	Entry->Value = *Value;

	BucketIndex = KeySignature % KEX_SMP_HASH_BUCKETS;
	Entry->Next = StringMapper->Buckets[BucketIndex];
	StringMapper->Buckets[BucketIndex] = Entry;

	return STATUS_SUCCESS;
}

STATIC NTSTATUS NTAPI SmppLookupRawEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER						StringMapper,
	IN		PCUNICODE_STRING							Key,
	OUT		PPKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY	EntryOut)
{
	BOOLEAN CaseInsensitive;
	PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Entry;
	ULONG KeySignature;
	ULONG BucketIndex;

	if (!StringMapper) {
		return STATUS_INVALID_PARAMETER_1;
	}

	if (!Key) {
		return STATUS_INVALID_PARAMETER_2;
	}

	if (!EntryOut) {
		return STATUS_INTERNAL_ERROR;
	}

	CaseInsensitive = (StringMapper->Flags & KEX_SMP_STRING_MAPPER_CASE_INSENSITIVE_KEYS) != 0;

	RtlHashUnicodeString(
		Key,
		CaseInsensitive,
		HASH_STRING_ALGORITHM_DEFAULT,
		&KeySignature);
		
	BucketIndex = KeySignature % KEX_SMP_HASH_BUCKETS;
	Entry = StringMapper->Buckets[BucketIndex];

	while (Entry) {
		if (Entry->Hash == KeySignature) {
			if (RtlEqualUnicodeString(Key, &Entry->Key, CaseInsensitive)) {
				*EntryOut = Entry;
				return STATUS_SUCCESS;
			}
		}
		Entry = Entry->Next;
	}

	*EntryOut = NULL;
	return STATUS_STRING_MAPPER_ENTRY_NOT_FOUND;
}

SMPAPI NTSTATUS NTAPI SmpLookupEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN		PCUNICODE_STRING				Key,
	OUT		PUNICODE_STRING					Value OPTIONAL)
{
	NTSTATUS Status;
	PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Entry;

	Status = SmppLookupRawEntryStringMapper(
		StringMapper,
		Key,
		&Entry);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if (Value) {
		*Value = Entry->Value;
	}

	return Status;
}

SMPAPI NTSTATUS NTAPI SmpRemoveEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN		PCUNICODE_STRING				Key)
{
	BOOLEAN CaseInsensitive;
	PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Entry, Prev;
	ULONG KeySignature;
	ULONG BucketIndex;

	if (!StringMapper) return STATUS_INVALID_PARAMETER_1;
	if (!Key) return STATUS_INVALID_PARAMETER_2;

	CaseInsensitive = (StringMapper->Flags & KEX_SMP_STRING_MAPPER_CASE_INSENSITIVE_KEYS) != 0;

	RtlHashUnicodeString(
		Key,
		CaseInsensitive,
		HASH_STRING_ALGORITHM_DEFAULT,
		&KeySignature);

	BucketIndex = KeySignature % KEX_SMP_HASH_BUCKETS;
	Entry = StringMapper->Buckets[BucketIndex];
	Prev = NULL;

	while (Entry) {
		if (Entry->Hash == KeySignature) {
			if (RtlEqualUnicodeString(Key, &Entry->Key, CaseInsensitive)) {
				if (Prev) {
					Prev->Next = Entry->Next;
				} else {
					StringMapper->Buckets[BucketIndex] = Entry->Next;
				}
				SafeFree(Entry);
				return STATUS_SUCCESS;
			}
		}
		Prev = Entry;
		Entry = Entry->Next;
	}

	return STATUS_STRING_MAPPER_ENTRY_NOT_FOUND;
}

SMPAPI NTSTATUS NTAPI SmpApplyStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	PUNICODE_STRING					KeyToValue)
{
	return SmpLookupEntryStringMapper(StringMapper, KeyToValue, KeyToValue);
}

SMPAPI NTSTATUS NTAPI SmpInsertMultipleEntriesStringMapper(
	IN		PKEX_SMP_STRING_MAPPER				StringMapper,
	IN		CONST KEX_SMP_STRING_MAPPER_ENTRY	Entries[],
	IN		ULONG								EntryCount)
{
	NTSTATUS FailureStatus;

	if (!StringMapper) return STATUS_INVALID_PARAMETER_1;
	if (!Entries) return STATUS_INVALID_PARAMETER_2;
	if (!EntryCount) return STATUS_INVALID_PARAMETER_3;

	FailureStatus = STATUS_SUCCESS;

	do {
		NTSTATUS Status;

		Status = SmpInsertEntryStringMapper(
			StringMapper,
			&Entries[EntryCount-1].Key,
			&Entries[EntryCount-1].Value);

		if (!NT_SUCCESS(Status)) {
			FailureStatus = Status;
		}
	} while (--EntryCount);

	return FailureStatus;
}

SMPAPI NTSTATUS NTAPI SmpLookupMultipleEntriesStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	KEX_SMP_STRING_MAPPER_ENTRY		Entries[],
	IN		ULONG							EntryCount)
{
	NTSTATUS FailureStatus;

	if (!StringMapper) return STATUS_INVALID_PARAMETER_1;
	if (!Entries) return STATUS_INVALID_PARAMETER_2;
	if (!EntryCount) return STATUS_INVALID_PARAMETER_3;

	FailureStatus = STATUS_SUCCESS;

	do {
		NTSTATUS Status;

		Status = SmpLookupEntryStringMapper(
			StringMapper,
			&Entries[EntryCount-1].Key,
			&Entries[EntryCount-1].Value);

		if (!NT_SUCCESS(Status)) {
			FailureStatus = Status;
		}
	} while (--EntryCount);

	return FailureStatus;
}

SMPAPI NTSTATUS NTAPI SmpBatchApplyStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	UNICODE_STRING					KeyToValue[],
	IN		ULONG							KeyToValueCount)
{
	NTSTATUS FailureStatus;

	if (!StringMapper) return STATUS_INVALID_PARAMETER_1;
	if (!KeyToValue) return STATUS_INVALID_PARAMETER_2;
	if (!KeyToValueCount) return STATUS_INVALID_PARAMETER_3;

	FailureStatus = STATUS_SUCCESS;

	do {
		NTSTATUS Status;

		Status = SmpApplyStringMapper(
			StringMapper,
			&KeyToValue[KeyToValueCount-1]);

		if (!NT_SUCCESS(Status)) {
			FailureStatus = Status;
		}
	} while (--KeyToValueCount);

	return FailureStatus;
}