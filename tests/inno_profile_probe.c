#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../00-Common-Headers/InnoProfile.h"

// Reads data only: never executes the installer or changes its file.
int main(int argc, char **argv) {
    FILE *File;
    BYTE *Raw, *Image;
    long RawSize;
    PIMAGE_NT_HEADERS32 Nt;
    PIMAGE_SECTION_HEADER Section;
    ULONG Size, I, Match, Base, Saved, CopyRva;
    BYTE CopyBytes[27];
    if (argc != 2) return 2;
    File = fopen(argv[1], "rb");
    if (!File) return 2;
    fseek(File, 0, SEEK_END); RawSize = ftell(File); rewind(File);
    Raw = (BYTE *)malloc(RawSize);
    if (!Raw || fread(Raw, 1, RawSize, File) != RawSize) return 2;
    fclose(File);
    if (RawSize < sizeof(IMAGE_DOS_HEADER) ||
        ((PIMAGE_DOS_HEADER)Raw)->e_lfanew <= 0 ||
        !InnoRange(RawSize, ((PIMAGE_DOS_HEADER)Raw)->e_lfanew, sizeof(*Nt))) return 2;
    Nt = (PIMAGE_NT_HEADERS32)(Raw + ((PIMAGE_DOS_HEADER)Raw)->e_lfanew);
    Size = Nt->OptionalHeader.SizeOfImage; Base = Nt->OptionalHeader.ImageBase;
    if (Size > 0x4000000 || Nt->OptionalHeader.SizeOfHeaders > (ULONG)RawSize ||
        Nt->OptionalHeader.SizeOfHeaders > Size) return 2;
    Image = (BYTE *)calloc(Size, 1);
    if (!Image) return 2;
    memcpy(Image, Raw, Nt->OptionalHeader.SizeOfHeaders);
    Section = IMAGE_FIRST_SECTION(Nt);
    for (I=0; I<Nt->FileHeader.NumberOfSections; ++I,++Section) {
        if (!InnoRange(RawSize,Section->PointerToRawData,Section->SizeOfRawData) ||
            !InnoRange(Size,Section->VirtualAddress,Section->SizeOfRawData)) return 2;
        memcpy(Image+Section->VirtualAddress,Raw+Section->PointerToRawData,Section->SizeOfRawData);
    }
    Match = InnoFindAvxInitialization(Image,Size,Base);
    printf("%s: match=%08lx\n",argv[1],Match);
    if (!Match) return 1;
    Nt = InnoImageHeaders(Image,Size);
    // Product build identity is irrelevant.
    Nt->FileHeader.TimeDateStamp ^= 0x12345678;
    if (InnoFindAvxInitialization(Image,Size,Base) != Match) return 1;
    // Missing attribution, out-of-image flag and truncated input must fail.
    Saved = Nt->OptionalHeader.DataDirectory[2].VirtualAddress;
    Nt->OptionalHeader.DataDirectory[2].VirtualAddress = 0;
    if (InnoFindAvxInitialization(Image,Size,Base)) return 1;
    Nt->OptionalHeader.DataDirectory[2].VirtualAddress = Saved;
    Saved = *(PULONG)(Image+Match+7);
    *(PULONG)(Image+Match+7) = Base+Size;
    if (InnoFindAvxInitialization(Image,Size,Base)) return 1;
    *(PULONG)(Image+Match+7) = Saved;
    if (InnoFindAvxInitialization(Image,Size-1,Base)) return 1;
    // Duplicate an otherwise valid initializer in executable space. Adjust
    // its call displacement, then require ambiguity to leave it untouched.
    Section = IMAGE_FIRST_SECTION(Nt);
    for (I=0; I<Nt->FileHeader.NumberOfSections; ++I,++Section) {
        if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE) || Section->Misc.VirtualSize < 128) continue;
        CopyRva = Section->VirtualAddress + Section->Misc.VirtualSize - 96;
        memcpy(CopyBytes,Image+CopyRva,sizeof(CopyBytes));
        memcpy(Image+CopyRva,Image+Match-16,sizeof(CopyBytes));
        *(PLONG)(Image+CopyRva+1) += (LONG)(Match-16-CopyRva);
        if (InnoFindAvxInitialization(Image,Size,Base)) return 1;
        memcpy(Image+CopyRva,CopyBytes,sizeof(CopyBytes));
        break;
    }
    Image[Match] ^= 1;
    if (InnoFindAvxInitialization(Image,Size,Base)) return 1;
    free(Image); free(Raw);
    puts("PASS: content recognition and negative checks");
    return 0;
}
