///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     KexSmp.h
//
// Abstract:
//
//     Public header file for the string mapper APIs.
//
// Author:
//
//     vxiiduu (17-May-2025)
//
// Environment:
//
//     Setup, Native, Win32
//
// Revision History:
//
//     vxiiduu               17-May-2025  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <KexComm.h>

//
// Macros
//

#ifndef SMPAPI
#  define SMPAPI
#  pragma comment(lib, "KexSmp.lib")
#endif

#define KEX_SMP_STRING_MAPPER_CASE_INSENSITIVE_KEYS 1
#define KEX_SMP_STRING_MAPPER_FLAGS_VALID_MASK (KEX_SMP_STRING_MAPPER_CASE_INSENSITIVE_KEYS)

//
// Data Types
//

#define KEX_SMP_HASH_BUCKETS 256

typedef struct _KEX_SMP_STRING_MAPPER_ENTRY {
	UNICODE_STRING	Key;
	UNICODE_STRING	Value;
} TYPEDEF_TYPE_NAME(KEX_SMP_STRING_MAPPER_ENTRY);

typedef struct _KEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY {
	struct _KEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY *Next;
	ULONG							Hash;
	UNICODE_STRING					Key;
	UNICODE_STRING					Value;
} TYPEDEF_TYPE_NAME(KEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY);

typedef struct _KEX_SMP_STRING_MAPPER {
	ULONG							Flags;
	PKEX_SMP_STRING_MAPPER_HASH_TABLE_ENTRY Buckets[KEX_SMP_HASH_BUCKETS];
} TYPEDEF_TYPE_NAME(KEX_SMP_STRING_MAPPER);


//
// Functions
//

SMPAPI NTSTATUS NTAPI SmpCreateStringMapper(
	OUT		PPKEX_SMP_STRING_MAPPER		StringMapper,
	IN		ULONG						Flags OPTIONAL);

SMPAPI NTSTATUS NTAPI SmpDeleteStringMapper(
	IN		PPKEX_SMP_STRING_MAPPER		StringMapper);

SMPAPI NTSTATUS NTAPI SmpInsertEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER		StringMapper,
	IN		PCUNICODE_STRING			Key,
	IN		PCUNICODE_STRING			Value);

SMPAPI NTSTATUS NTAPI SmpLookupEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN		PCUNICODE_STRING				Key,
	OUT		PUNICODE_STRING					Value OPTIONAL);

SMPAPI NTSTATUS NTAPI SmpRemoveEntryStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN		PCUNICODE_STRING				Key);

SMPAPI NTSTATUS NTAPI SmpApplyStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	PUNICODE_STRING					KeyToValue);

SMPAPI NTSTATUS NTAPI SmpInsertMultipleEntriesStringMapper(
	IN		PKEX_SMP_STRING_MAPPER				StringMapper,
	IN		CONST KEX_SMP_STRING_MAPPER_ENTRY	Entries[],
	IN		ULONG								EntryCount);

SMPAPI NTSTATUS NTAPI SmpLookupMultipleEntriesStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	KEX_SMP_STRING_MAPPER_ENTRY		Entries[],
	IN		ULONG							EntryCount);

SMPAPI NTSTATUS NTAPI SmpBatchApplyStringMapper(
	IN		PKEX_SMP_STRING_MAPPER			StringMapper,
	IN OUT	UNICODE_STRING					KeyToValue[],
	IN		ULONG							KeyToValueCount);