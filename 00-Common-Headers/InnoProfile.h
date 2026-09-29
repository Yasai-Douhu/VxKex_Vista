#pragma once

// Content-based recognition of the x86 Delphi AVX Move dispatcher used by
// Inno Setup. Addresses are checked against PE sections, never filenames/RVAs.
static BOOL InnoRange(ULONG Size, ULONG Rva, ULONG Length) {
    return Rva <= Size && Length <= Size - Rva;
}

static PIMAGE_NT_HEADERS32 InnoImageHeaders(PBYTE Image, ULONG Size) {
    PIMAGE_DOS_HEADER Dos;
    PIMAGE_NT_HEADERS32 Nt;
    ULONG Sections;
    if (Size < sizeof(IMAGE_DOS_HEADER)) return NULL;
    Dos = (PIMAGE_DOS_HEADER)Image;
    if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0 ||
        !InnoRange(Size, (ULONG)Dos->e_lfanew, sizeof(*Nt))) return NULL;
    Nt = (PIMAGE_NT_HEADERS32)(Image + Dos->e_lfanew);
    if (Nt->Signature != IMAGE_NT_SIGNATURE ||
        Nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        Nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        Nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32) ||
        Nt->OptionalHeader.SizeOfImage != Size) return NULL;
    Sections = (ULONG)((PBYTE)IMAGE_FIRST_SECTION(Nt) - Image);
    if (!InnoRange(Size, Sections, Nt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER))) return NULL;
    return Nt;
}

static BOOL InnoSectionHas(PIMAGE_NT_HEADERS32 Nt, ULONG Rva, ULONG Length, ULONG Flags) {
    PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(Nt);
    ULONG I;
    if (!InnoRange(Nt->OptionalHeader.SizeOfImage, Rva, Length)) return FALSE;
    for (I = 0; I < Nt->FileHeader.NumberOfSections; ++I, ++Section) {
        if ((Section->Characteristics & Flags) == Flags && Rva >= Section->VirtualAddress &&
            InnoRange(Section->Misc.VirtualSize, Rva - Section->VirtualAddress, Length)) return TRUE;
    }
    return FALSE;
}

static BOOL InnoHasAttribution(PBYTE Image, ULONG Size) {
    static const WCHAR Marker[] = L"This installation was built with Inno Setup.";
    PIMAGE_NT_HEADERS32 Nt = InnoImageHeaders(Image, Size);
    ULONG Rva, Length, I;
    if (!Nt || Nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) return FALSE;
    Rva = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
    Length = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size;
    if (!Rva || !InnoRange(Size, Rva, Length) || Length < sizeof(Marker)) return FALSE;
    for (I = 0; I <= Length - sizeof(Marker); ++I)
        if (!memcmp(Image + Rva + I, Marker, sizeof(Marker))) return TRUE;
    return FALSE;
}

static BOOL InnoHasAvxConsumer(PBYTE Image, PIMAGE_NT_HEADERS32 Nt, ULONG Flag) {
    static const BYTE Avx[] = {0xc5,0xfc,0x10,0x08,0xc5,0xfc,0x10,0x54,0x08,0xe0};
    PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(Nt);
    ULONG I, J, Length, Rva, Target;
    PBYTE Code;
    for (I = 0; I < Nt->FileHeader.NumberOfSections; ++I, ++Section) {
        Rva = Section->VirtualAddress; Length = Section->Misc.VirtualSize;
        if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE) ||
            !InnoRange(Nt->OptionalHeader.SizeOfImage, Rva, Length) || Length < 16) continue;
        for (J = 0; J <= Length - 16; ++J) {
            Code = Image + Rva + J;
            // test dword ptr [flag],1; jne AVX_Move
            if (Code[0] != 0xf7 || Code[1] != 0x05 || *(PULONG)(Code+2) != Flag ||
                *(PULONG)(Code+6) != 1 || Code[10] != 0x0f || Code[11] != 0x85) continue;
            Target = Rva + J + 16 + *(PLONG)(Code+12);
            if (InnoSectionHas(Nt, Target, sizeof(Avx), IMAGE_SCN_MEM_EXECUTE) &&
                !memcmp(Image+Target, Avx, sizeof(Avx))) return TRUE;
        }
    }
    return FALSE;
}

static ULONG InnoFindAvxInitialization(PBYTE Image, ULONG Size, ULONG ImageBase) {
    PIMAGE_NT_HEADERS32 Nt = InnoImageHeaders(Image, Size);
    PIMAGE_SECTION_HEADER Section;
    PBYTE Code;
    ULONG I, J, Rva, Length, CpuByte, Flag, Found = 0, Gate;
    static const BYTE Store[] = {0x0f,0x95,0xc0,0x83,0xe0,0x7f,0xa3};
    if (!Nt || !InnoHasAttribution(Image, Size)) return 0;
    Section = IMAGE_FIRST_SECTION(Nt);
    for (I = 0; I < Nt->FileHeader.NumberOfSections; ++I, ++Section) {
        Rva = Section->VirtualAddress; Length = Section->Misc.VirtualSize;
        if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE) ||
            !InnoRange(Size, Rva, Length) || Length < 27) continue;
        for (J = 0; J <= Length - 27; ++J) {
            Code = Image + Rva + J;
            // call OS_gate; test al,al; je fallback; test [CPUID.ECX+3],10h;
            // setne al; and eax,7fh; mov [AVX_dispatch_flag],eax.
            if (Code[0] != 0xe8 || Code[5] != 0x84 || Code[6] != 0xc0 ||
                Code[7] != 0x74 || Code[9] != 0xf6 || Code[10] != 0x05 ||
                Code[15] != 0x10 || memcmp(Code+16, Store, sizeof(Store))) continue;
            CpuByte = *(PULONG)(Code+11); Flag = *(PULONG)(Code+23);
            Gate = Rva + J + 5 + *(PLONG)(Code+1);
            if (CpuByte < ImageBase || Flag < ImageBase ||
                !InnoSectionHas(Nt, CpuByte-ImageBase, 1, IMAGE_SCN_MEM_WRITE) ||
                !InnoSectionHas(Nt, Flag-ImageBase, 4, IMAGE_SCN_MEM_WRITE) ||
                !InnoSectionHas(Nt, Gate, 1, IMAGE_SCN_MEM_EXECUTE) ||
                !InnoSectionHas(Nt, Rva+J+9+(CHAR)Code[8], 1, IMAGE_SCN_MEM_EXECUTE) ||
                !InnoHasAvxConsumer(Image, Nt, Flag)) continue;
            // Ambiguous code is left untouched rather than choosing a match.
            if (Found) return 0;
            Found = Rva + J + 16;
        }
    }
    return Found;
}
