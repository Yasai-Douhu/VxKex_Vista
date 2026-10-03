#pragma once

// A conservative content profile, not compiler authentication. Require the
// build-id section AND the modern NT-I/O import signature. No product/version
// names or addresses. Input is a mapped image (not a raw PE file).
static BOOL ZigIoRange(ULONG Size, ULONG Rva, ULONG Length) {
    return Rva <= Size && Length <= Size - Rva;
}

static BOOL ZigIoName(PBYTE Image, ULONG Size, ULONG Rva, const char *Name) {
    ULONG Length = (ULONG)strlen(Name) + 1;
    return ZigIoRange(Size, Rva, Length) && !memcmp(Image + Rva, Name, Length);
}

static BOOL ZigIoModuleName(PBYTE Image, ULONG Size, ULONG Rva, const char *Name) {
    ULONG Length = (ULONG)strlen(Name) + 1;
    return ZigIoRange(Size, Rva, Length) && !_strnicmp((LPCSTR)Image+Rva, Name, Length);
}

static BOOL ZigNtIoImage(PBYTE Image, ULONG Size) {
    PIMAGE_DOS_HEADER Dos;
    PIMAGE_FILE_HEADER File;
    PIMAGE_SECTION_HEADER Sections;
    ULONG Pe, Opt, Table, I, J, ImportRva, ImportSize, Bits, Mask = 0;
    ULONG DirOffset, DirCountOffset;
    USHORT Magic;
    BOOL BuildId = FALSE, End = FALSE;
    if (Size < sizeof(IMAGE_DOS_HEADER)) return FALSE;
    Dos = (PIMAGE_DOS_HEADER)Image;
    if (Dos->e_magic != IMAGE_DOS_SIGNATURE || Dos->e_lfanew <= 0) return FALSE;
    Pe = (ULONG)Dos->e_lfanew;
    if (!ZigIoRange(Size, Pe, 24) || *(PULONG)(Image+Pe) != IMAGE_NT_SIGNATURE) return FALSE;
    File = (PIMAGE_FILE_HEADER)(Image+Pe+4); Opt = Pe+24;
    if (!ZigIoRange(Size, Opt, File->SizeOfOptionalHeader) || File->SizeOfOptionalHeader < 2) return FALSE;
    Magic = *(PUSHORT)(Image+Opt);
    if (Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC && File->Machine == IMAGE_FILE_MACHINE_I386) {
        Bits = 4; DirOffset = 96; DirCountOffset = 92;
    } else if (Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC && File->Machine == IMAGE_FILE_MACHINE_AMD64) {
        Bits = 8; DirOffset = 112; DirCountOffset = 108;
    } else return FALSE;
    if (File->SizeOfOptionalHeader < DirOffset+16 || *(PULONG)(Image+Opt+56) != Size ||
        *(PULONG)(Image+Opt+DirCountOffset) < 2) return FALSE;
    Table = Opt+File->SizeOfOptionalHeader;
    if (!ZigIoRange(Size, Table, File->NumberOfSections*sizeof(IMAGE_SECTION_HEADER))) return FALSE;
    Sections = (PIMAGE_SECTION_HEADER)(Image+Table);
    for (I=0; I<File->NumberOfSections; ++I) {
        if (!memcmp(Sections[I].Name, ".buildid", 8) &&
            Sections[I].Misc.VirtualSize && (Sections[I].Characteristics & IMAGE_SCN_MEM_READ) &&
            ZigIoRange(Size, Sections[I].VirtualAddress, Sections[I].Misc.VirtualSize)) BuildId = TRUE;
    }
    if (!BuildId) return FALSE;
    ImportRva = *(PULONG)(Image+Opt+DirOffset+8);
    ImportSize = *(PULONG)(Image+Opt+DirOffset+12);
    if (!ImportRva || !ZigIoRange(Size, ImportRva, ImportSize)) return FALSE;
    for (I=0; ImportSize-I >= sizeof(IMAGE_IMPORT_DESCRIPTOR); I+=sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        PIMAGE_IMPORT_DESCRIPTOR D = (PIMAGE_IMPORT_DESCRIPTOR)(Image+ImportRva+I);
        ULONG Rva;
        if (!D->Name && !D->FirstThunk && !D->OriginalFirstThunk) { End = TRUE; break; }
        if (!ZigIoRange(Size, D->Name, 1)) return FALSE;
        // The normal VxKex rewrite mapper emits "kxnt" without an extension.
        // Require the complete bounded name, including NUL, in either form.
        if (!ZigIoModuleName(Image,Size,D->Name,"ntdll.dll") &&
            !ZigIoModuleName(Image,Size,D->Name,"ntdll") &&
            !ZigIoModuleName(Image,Size,D->Name,"kxnt.dll") &&
            !ZigIoModuleName(Image,Size,D->Name,"kxnt")) continue;
        if (!D->OriginalFirstThunk) continue; // IAT may already contain relocated addresses.
        Rva = D->OriginalFirstThunk;
        for (J=0; ZigIoRange(Size, Rva, Bits); ++J) {
            ULONGLONG Entry = Bits == 4 ? *(PULONG)(Image+Rva) : *(PULONGLONG)(Image+Rva);
            ULONG Name;
            if (!Entry) break;
            if (!(Entry & (Bits == 4 ? IMAGE_ORDINAL_FLAG32 : IMAGE_ORDINAL_FLAG64))) {
                if (Entry > 0xffffffffUL-2) return FALSE;
                Name = (ULONG)Entry+2;
                if (ZigIoName(Image, Size, Name, "NtWriteFile")) Mask |= 1;
                if (ZigIoName(Image, Size, Name, "NtWaitForAlertByThreadId")) Mask |= 2;
                if (ZigIoName(Image, Size, Name, "RtlReportSilentProcessExit")) Mask |= 4;
            }
            if (Rva > 0xffffffffUL-Bits) return FALSE;
            Rva += Bits;
        }
        if (!ZigIoRange(Size, Rva, Bits)) return FALSE;
    }
    return End && Mask == 7;
}
