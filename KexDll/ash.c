///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     ash.c
//
// Abstract:
//
//     This file contains helper routines for app-specific hacks.
//
// Author:
//
//     vxiiduu (16-Feb-2024)
//
// Environment:
//
//     Native mode
//
// Revision History:
//
//     vxiiduu              16-Feb-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kexdllp.h"

#ifndef _WIN64
#include <InnoProfile.h>
STATIC ULONG AshpControlsIatRva;
STATIC VOID WINAPI AshpInnoInitCommonControls(VOID)
{
	PBYTE Image = (PBYTE) NtCurrentPeb()->ImageBaseAddress;
	ACTCTX_SECTION_KEYED_DATA Data;
	BYTE Information[2048];
	SIZE_T Required;
	WCHAR Path[MAX_PATH];
	PACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION Assembly;
	HMODULE Module;
	BOOL (WINAPI *RegisterName)(PCWSTR);
	static const PCWSTR Classes[] = {L"Static",L"Button",L"ListBox",L"ComboBox",L"Edit",L"ScrollBar"};
	ULONG Index;
	// The original IAT entry is resolved by the loader before this thunk runs.
	((VOID (WINAPI *)(VOID)) *(PPVOID)(Image + AshpControlsIatRva))();
	RtlZeroMemory(&Data, sizeof(Data));
	Data.cbSize = sizeof(Data);
	if (!FindActCtxSectionStringW(FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX,
		NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION,L"Static",&Data)) return;
	if (QueryActCtxW(0,Data.hActCtx,&Data.ulAssemblyRosterIndex,
		AssemblyDetailedInformationInActivationContext,Information,sizeof(Information),&Required)) {
		Assembly = (PACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION) Information;
		// Use the assembly selected by the OS, not a hardcoded servicing version.
		if (Assembly->lpAssemblyDirectoryName &&
			StringBeginsWithI(Assembly->lpAssemblyDirectoryName,L"x86_microsoft.windows.common-controls_") &&
			!wcschr(Assembly->lpAssemblyDirectoryName,L'\\') &&
			!wcschr(Assembly->lpAssemblyDirectoryName,L'/') &&
			SUCCEEDED(StringCchPrintfW(Path,ARRAYSIZE(Path),L"%wZ\\WinSxS\\%s\\comctl32.dll",
				&KexData->WinDir,Assembly->lpAssemblyDirectoryName))) {
			Module = LoadLibraryW(Path);
			if (Module) {
				RegisterName = (PVOID)GetProcAddress(Module,"RegisterClassNameW");
				if (RegisterName) for (Index=0;Index<ARRAYSIZE(Classes);++Index) RegisterName(Classes[Index]);
				// Keep the module loaded while its registered window procedures exist.
			}
		}
	}
	ReleaseActCtx(Data.hActCtx);
}

// Identify the setup body by its version-resource attribution, then locate
// its import thunks. Do not use executable names or build-specific addresses.
STATIC VOID AshpApplyInnoControlsProfile(VOID)
{
	PBYTE Image = (PBYTE)NtCurrentPeb()->ImageBaseAddress;
	PIMAGE_DOS_HEADER Dos = (PIMAGE_DOS_HEADER)Image;
	PIMAGE_NT_HEADERS32 Nt;
	PIMAGE_IMPORT_DESCRIPTOR Import;
	PIMAGE_SECTION_HEADER Section;
	ULONG Size, Rva, Length, Offset, Index, Slot, Thunk, Iat, Count, CodeSize;
	if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0 || Dos->e_lfanew > 0x1000) return;
	Nt = (PIMAGE_NT_HEADERS32)(Image + Dos->e_lfanew);
	if (Nt->Signature != IMAGE_NT_SIGNATURE || Nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
		Nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
		Nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) return;
	Size = Nt->OptionalHeader.SizeOfImage;
	if (!InnoHasAttribution(Image, Size)) return;
	// Record detection independently of the controls import/thunk layout.
	// KxBase uses this to preserve Inno's Vista DLL-search fallback.
	KexData->Flags |= KEXDATA_FLAG_INNO_SETUP;
	Rva = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	Length = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	if (!Rva || Rva>=Size || Length>Size-Rva) return;
	for (Offset=0; Offset+sizeof(*Import)<=Length; Offset+=sizeof(*Import)) {
		Import=(PIMAGE_IMPORT_DESCRIPTOR)(Image+Rva+Offset);
		if (!Import->Name) break;
		if (Import->Name>=Size || !memchr(Image+Import->Name,0,Size-Import->Name) ||
			_stricmp((PCSTR)(Image+Import->Name),"comctl32.dll")) continue;
		if (!Import->OriginalFirstThunk || Import->OriginalFirstThunk>=Size ||
			!Import->FirstThunk || Import->FirstThunk>=Size) return;
		Count = min((Size-Import->OriginalFirstThunk)/4,(Size-Import->FirstThunk)/4);
		for (Slot=0; Slot<Count; ++Slot) {
			Thunk=*(PULONG)(Image+Import->OriginalFirstThunk+Slot*4);
			if (!Thunk) break;
			if (Thunk & IMAGE_ORDINAL_FLAG32 || Thunk>=Size || Size-Thunk<2+sizeof("InitCommonControls")) continue;
			if (memcmp(Image+Thunk+2,"InitCommonControls",sizeof("InitCommonControls"))) continue;
			Iat=Import->FirstThunk+Slot*4;
			Section=IMAGE_FIRST_SECTION(Nt);
			for (Index=0; Index<Nt->FileHeader.NumberOfSections; ++Index,++Section) {
				if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE) || Section->VirtualAddress>=Size) continue;
				CodeSize=min(Section->Misc.VirtualSize,Size-Section->VirtualAddress);
				for (Length=0; Length+6<=CodeSize; ++Length) {
					PBYTE Code=Image+Section->VirtualAddress+Length;
					if (Code[0]==0xff && Code[1]==0x25 && *(PULONG)(Code+2)==(ULONG)(Image+Iat)) {
						AshpControlsIatRva=Iat;
						KexHkInstallBasicHook(Code,AshpInnoInitCommonControls,NULL);
						Length+=5;
					}
				}
			}
			return;
		}
	}
}
#endif

// Delphi delay loading uses PE delay descriptors, independently of regular
// import rewriting. Discover the USER32 name through that table, not a RVA.
STATIC VOID AshpRedirectInnoDelayImports(PBYTE Image, ULONG ImageSize)
{
#ifndef _WIN64
	PIMAGE_NT_HEADERS32 Nt = InnoImageHeaders(Image, ImageSize);
	ULONG Rva, Length, Offset, Name, OldProtection, Ignored;
	PULONG Descriptor;
	PVOID Region;
	SIZE_T Size;
	NTSTATUS Status;
	if (!Nt || Nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) return;
	Rva = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress;
	Length = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size;
	if (!Rva || !InnoRange(ImageSize, Rva, Length)) return;
	for (Offset = 0; Length >= 32 && Offset <= Length-32; Offset += 32) {
		Descriptor = (PULONG)(Image + Rva + Offset);
		if (!Descriptor[1]) break;
		// Both old VA descriptors and current RVA descriptors are defined by PE.
		if (Descriptor[0] & ~1UL) continue;
		Name = Descriptor[1];
		if (!(Descriptor[0] & 1)) {
			if (Name < (ULONG)Image) continue;
			Name -= (ULONG)Image;
		}
		if (!InnoRange(ImageSize, Name, sizeof("user32.dll")) ||
			_strnicmp((PCSTR)(Image+Name), "user32.dll", sizeof("user32.dll"))) continue;
		Region = Image + Name;
		Size = sizeof("user32.dll");
		Status = NtProtectVirtualMemory(NtCurrentProcess(), &Region, &Size,
			PAGE_READWRITE, &OldProtection);
		if (!NT_SUCCESS(Status)) continue;
		RtlCopyMemory(Image+Name, "kxuser.dll", sizeof("kxuser.dll"));
		NtProtectVirtualMemory(NtCurrentProcess(), &Region, &Size, OldProtection, &Ignored);
		KexLogInformationEvent(L"Inno profile: redirected USER32 delay imports at RVA %08lx", Name);
	}
#endif
}

// Inno attribution + linked code/data patterns identify the Delphi runtime.
// Keep its SSE path on NT 6.0 regardless of product, version or filename.
VOID AshApplyInnoSetupWorkarounds(VOID)
{
#ifndef _WIN64
	PBYTE Image;
	PIMAGE_DOS_HEADER Dos;
	PIMAGE_NT_HEADERS32 Nt;
	PVOID Region;
	SIZE_T Size;
	ULONG OldProtection, Ignored;
	NTSTATUS Status;
	ULONG PatchRva, ImageSize;

	if (OriginalMajorVersion != 6 || OriginalMinorVersion != 0) return;
	AshpApplyInnoControlsProfile();
	Image = (PBYTE) NtCurrentPeb()->ImageBaseAddress;
	Dos = (PIMAGE_DOS_HEADER) Image;
	if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0 ||
		Dos->e_lfanew > 0x1000) return;
	Nt = (PIMAGE_NT_HEADERS32) (Image + Dos->e_lfanew);
	ImageSize = Nt->OptionalHeader.SizeOfImage;
	if (!InnoHasAttribution(Image, ImageSize)) return;
	AshpRedirectInnoDelayImports(Image, ImageSize);
	PatchRva = InnoFindAvxInitialization(Image, ImageSize, (ULONG)Image);
	if (!PatchRva) return;
	Region = Image + PatchRva;
	Size = 3;
	Status = NtProtectVirtualMemory(NtCurrentProcess(), &Region, &Size,
		PAGE_EXECUTE_READWRITE, &OldProtection);
	if (!NT_SUCCESS(Status)) return;
	Image[PatchRva] = 0x30; // xor al,al; nop (replaces setne al)
	Image[PatchRva+1] = 0xc0;
	Image[PatchRva+2] = 0x90;
	NtFlushInstructionCache(NtCurrentProcess(), Image + PatchRva, 3);
	Status = NtProtectVirtualMemory(NtCurrentProcess(), &Region, &Size,
		OldProtection, &Ignored);
	KexLogInformationEvent(L"Inno installer profile: disabled AVX copy dispatch on NT 6.0 (protection restore: %08lx)", Status);
#endif
}

//
// ExeName must include the .exe extension.
//
KEXAPI BOOLEAN NTAPI AshExeBaseNameIs(
	IN	PCWSTR	ExeName)
{
	NTSTATUS Status;
	UNICODE_STRING ExeNameUS;

	ASSERT (KexData != NULL);

	Status = RtlInitUnicodeStringEx(&ExeNameUS, ExeName);
	ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	return RtlEqualUnicodeString(&KexData->ImageBaseName, &ExeNameUS, TRUE);
}

//
// This function is intended to be used like this:
//
//   if (AshModuleBaseNameIs(ReturnAddress(), L"kernel32.dll"))
//
// File extension (.dll, .exe etc.) is required.
//
KEXAPI BOOLEAN NTAPI AshModuleBaseNameIs(
	IN	PVOID	AddressInsideModule,
	IN	PCWSTR	ModuleName)
{
	NTSTATUS Status;
	UNICODE_STRING DllFullPath;
	UNICODE_STRING DllBaseName;
	UNICODE_STRING ComparisonBaseName;

	RtlInitEmptyUnicodeStringFromTeb(&DllFullPath);

	//
	// Get the name of the DLL in which the specified address resides.
	//

	Status = KexLdrGetDllFullNameFromAddress(
		AddressInsideModule,
		&DllFullPath);

	ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	//
	// Convert full path into base name.
	//

	Status = KexRtlPathFindFileName(&DllFullPath, &DllBaseName);
	ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	Status = RtlInitUnicodeStringEx(&ComparisonBaseName, ModuleName);
	ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	return RtlEqualUnicodeString(&DllBaseName, &ComparisonBaseName, TRUE);
}

//
// As with AshModuleBaseNameIs, this is designed to be used with the
// ReturnAddress() macro as the argument.
//
KEXAPI BOOLEAN NTAPI AshModuleIsWindowsModule(
	IN	PVOID	AddressInsideModule)
{
	NTSTATUS Status;
	UNICODE_STRING DllFullPath;

	RtlInitEmptyUnicodeStringFromTeb(&DllFullPath);

	//
	// Get the name of the DLL in which the specified address resides.
	//

	Status = KexLdrGetDllFullNameFromAddress(
		AddressInsideModule,
		&DllFullPath);

	ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	//
	// See if it starts with %SystemRoot%.
	//

	if (RtlPrefixUnicodeString(&KexData->WinDir, &DllFullPath, TRUE)) {
		UNICODE_STRING SlashTemp;
		KexRtlAdvanceUnicodeString(&DllFullPath, KexData->WinDir.Length);
		RtlInitConstantUnicodeString(&SlashTemp, L"\\Temp");
		if (RtlPrefixUnicodeString(&SlashTemp, &DllFullPath, TRUE)) return FALSE;
		return TRUE;
	} else {
		return FALSE;
	}
}

VOID AshApplyQBittorrentEnvironmentVariableHacks(
	VOID)
{
	UNICODE_STRING VariableName;
	UNICODE_STRING VariableValue;

	ASSERT (AshExeBaseNameIs(L"qbittorrent.exe"));

	//
	// APPSPECIFICHACK: Applying the environment variable below will eliminate
	// the problem of bad kerning from qBittorrent. If more Qt apps are found
	// which have bad kerning, this may help fix those too.
	//

	KexLogInformationEvent(L"App-Specific Hack applied for qBittorrent");
	RtlInitConstantUnicodeString(&VariableName, L"QT_SCALE_FACTOR");
	RtlInitConstantUnicodeString(&VariableValue, L"1.0000001");
	RtlSetEnvironmentVariable(NULL, &VariableName, &VariableValue);
}

VOID AshApplyPythonEnvironmentVariableHacks(
	VOID)
{
	UNICODE_STRING VariableName;
	UNICODE_STRING VariableValue;

	ASSERT (StringBeginsWithI(KexData->ImageBaseName.Buffer, L"python"));

	if (LOWORD(OriginalBuildNumber) < 10586) {
		KexLogInformationEvent(L"App-Specific Hack applied for Python");
		RtlInitConstantUnicodeString(&VariableName, L"PYTHON_BASIC_REPL");
		RtlInitConstantUnicodeString(&VariableValue, L"1");
		RtlSetEnvironmentVariable(NULL, &VariableName, &VariableValue);
	}
}

VOID AshApplyNodeJSEnvironmentVariableHacks(
	VOID)
{
	UNICODE_STRING VariableName;
	UNICODE_STRING VariableValue;

	ASSERT (AshExeBaseNameIs(L"node.exe"));

	if (OriginalMajorVersion < 10) {
		KexLogInformationEvent(L"App-Specific Hack applied for Node.js");
		RtlInitConstantUnicodeString(&VariableName, L"NODE_SKIP_PLATFORM_CHECK");
		RtlInitConstantUnicodeString(&VariableValue, L"1");
		RtlSetEnvironmentVariable(NULL, &VariableName, &VariableValue);
	}
}
