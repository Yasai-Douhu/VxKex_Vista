// Ported from VxKex NEXT 1.2.3.2463, KexDll/kexrtl.c.
#include "buildcfg.h"
#include "kexdllp.h"
#include <inaddr.h>
#include <in6addr.h>

// The SDK bundled with VS2010 lacks ip2string.h; these are native NT 6.0
// exports, with the signatures documented by Microsoft's ip2string API.
NTSYSAPI NTSTATUS NTAPI RtlIpv4StringToAddressExW(PCWSTR, BOOLEAN, IN_ADDR *, PUSHORT);
NTSYSAPI NTSTATUS NTAPI RtlIpv6StringToAddressExW(PCWSTR, IN6_ADDR *, PULONG, PUSHORT);
NTSYSAPI NTSTATUS NTAPI RtlIpv4AddressToStringExW(const IN_ADDR *, USHORT, PWSTR, PULONG);
NTSYSAPI NTSTATUS NTAPI RtlIpv6AddressToStringExW(const IN6_ADDR *, ULONG, USHORT, PWSTR, PULONG);

static BOOLEAN KexIsMappedIpv4(const IN6_ADDR *Address)
{
    static const UCHAR prefix[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
    return memcmp(Address->u.Byte, prefix, sizeof(prefix)) == 0;
}

KEXAPI NTSTATUS NTAPI KexRtlCanonicalizeDomainName(
	OUT	PUNICODE_STRING		DestinationString,
	IN	PCUNICODE_STRING	SourceString,
	IN	BOOLEAN				Strict)
{
	BOOLEAN Success;
	NTSTATUS Status;
	ULONG Index;
	ULONG ScopeId;
	UNICODE_STRING RawName;
	USHORT Port;
	IN_ADDR Ipv4Address;
	IN6_ADDR Ipv6Address;
	ULONG CanonicalNameLength;
	ULONG PunycodedNameLength;
	WCHAR CanonicalNameBuffer[256];
	WCHAR PunycodedNameBuffer[256];
	WCHAR RawNameBuffer[256];

	if (!DestinationString || !SourceString ||
		!SourceString->Buffer ||
		(SourceString->Length % sizeof(WCHAR)) != 0 ||
		SourceString->Length > SourceString->MaximumLength) {
		return STATUS_INVALID_PARAMETER;
	}

	CanonicalNameLength = ARRAYSIZE(CanonicalNameBuffer);
	PunycodedNameLength = ARRAYSIZE(PunycodedNameBuffer);

	RtlInitEmptyUnicodeString(&RawName, RawNameBuffer, sizeof(RawNameBuffer));
	RtlCopyUnicodeString(&RawName, SourceString);

	if (RawName.Length == RawName.MaximumLength) {
		return STATUS_INVALID_IDN_NORMALIZATION;
	}

	//
	// Try parse as IPv6.
	//

	Status = RtlIpv6StringToAddressExW(
		RawName.Buffer,
		&Ipv6Address,
		&ScopeId,
		&Port);

	if (NT_SUCCESS(Status) && Port == 0) {
		//
		// We could parse this address as IPv6.
		// Convert the IPv6 struct back into a string - as an IPv6 string if it
		// is a true IPv6 address, or an IPv4 string if it is a mapped IPv4 address.
		//

		if (KexIsMappedIpv4(&Ipv6Address) && ScopeId == 0) {
			// Convert the IPv6-formatted IPv4 address into a real IPv4 address
			RtlCopyMemory(
				&Ipv4Address,
				&Ipv6Address.u.Byte[12],
				sizeof(Ipv4Address));

			Status = RtlIpv4AddressToStringExW(
				&Ipv4Address,
				Port,
				CanonicalNameBuffer,
				&CanonicalNameLength);
		} else {
			Status = RtlIpv6AddressToStringExW(
				&Ipv6Address,
				ScopeId,
				Port,
				CanonicalNameBuffer,
				&CanonicalNameLength);
		}

		if (!NT_SUCCESS(Status)) {
			return Status;
		}

		Success = RtlCreateUnicodeString(DestinationString, CanonicalNameBuffer);
		Status = Success ? STATUS_SUCCESS : STATUS_NO_MEMORY;
		return Status;
	}

	//
	// Try parse as IPv4.
	//

	Status = RtlIpv4StringToAddressExW(
		RawName.Buffer,
		Strict,
		&Ipv4Address,
		&Port);

	if (NT_SUCCESS(Status) && Port == 0) {
		//
		// We could parse the string as IPv4. Convert it back to a string.
		//

		Status = RtlIpv4AddressToStringExW(
			&Ipv4Address,
			Port,
			CanonicalNameBuffer,
			&CanonicalNameLength);

		if (!NT_SUCCESS(Status)) {
			return Status;
		}

		Success = RtlCreateUnicodeString(DestinationString, CanonicalNameBuffer);
		Status = Success ? STATUS_SUCCESS : STATUS_NO_MEMORY;
		return Status;
	}

	//
	// Try parse as IDN (internationalized domain name), and convert to punycode
	//

	Status = RtlIdnToAscii(
		0,
		SourceString->Buffer,
		KexRtlUnicodeStringCch(SourceString),
		PunycodedNameBuffer,
		&PunycodedNameLength);

	if (!NT_SUCCESS(Status)) {
		// NT 6.0 reports malformed UTF-16 as NO_UNICODE_TRANSLATION.
		// Modern RtlCanonicalizeDomainName reports INVALID_IDN_NORMALIZATION.
		return Status == STATUS_NO_UNICODE_TRANSLATION ? STATUS_INVALID_IDN_NORMALIZATION : Status;
	}

	//
	// Lowercase the Punycode representation
	//

	for (Index = 0; Index < PunycodedNameLength; ++Index) {
		// Note: we're using towlower (ntdll CRT) instead of ToLower (vxkex macro)
		// because ToLower does not handle non-ASCII characters.
		PunycodedNameBuffer[Index] = towlower(PunycodedNameBuffer[Index]);
	}

	//
	// Convert it back to proper Unicode
	//

	Status = RtlIdnToUnicode(
		0,
		PunycodedNameBuffer,
		PunycodedNameLength,
		CanonicalNameBuffer,
		&CanonicalNameLength);

	if (!NT_SUCCESS(Status)) {
		// NT 6.0 reports malformed UTF-16 as NO_UNICODE_TRANSLATION.
		// Modern RtlCanonicalizeDomainName reports INVALID_IDN_NORMALIZATION.
		return Status == STATUS_NO_UNICODE_TRANSLATION ? STATUS_INVALID_IDN_NORMALIZATION : Status;
	}

	if (CanonicalNameLength >= ARRAYSIZE(CanonicalNameBuffer)) {
		// potential buffer overflow
		return STATUS_INVALID_IDN_NORMALIZATION;
	}

	// Ensure null termination.
	// I'm not sure whether RtlIdnToUnicode guarantees a null terminated buffer,
	// but since it works with explicit length variables, it probably doesn't.
	// Win10 code does do this so it's probably required.
	CanonicalNameBuffer[CanonicalNameLength] = '\0';

	Success = RtlCreateUnicodeString(DestinationString, CanonicalNameBuffer);
	Status = Success ? STATUS_SUCCESS : STATUS_NO_MEMORY;
	return Status;
}
