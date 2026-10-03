#include "buildcfg.h"
#include "kexdllp.h"

typedef NTSTATUS (NTAPI *KEX_UTF_CONVERT)(PVOID,ULONG,PULONG,PCVOID,ULONG);
static KEX_UTF_CONVERT KexNativeUtfConvert(PCSTR Name,PVOID volatile *Cache) {
    PVOID Address=InterlockedCompareExchangePointer(Cache,NULL,NULL);
    PVOID Missing=(PVOID)(LONG_PTR)-1;
    if(!Address) {
        PTEB Teb=NtCurrentTeb();ULONG Error=Teb->LastErrorValue;
        NTSTATUS LastStatus=Teb->LastStatusValue;
        try {Address=(PVOID)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),Name);}
        finally {Teb->LastErrorValue=Error;Teb->LastStatusValue=LastStatus;}
        if(!Address)Address=Missing;
        InterlockedCompareExchangePointer(Cache,Address,NULL);
    }
    return Address==Missing?NULL:(KEX_UTF_CONVERT)Address;
}

// Consume an invalid prefix as native RTL does, leaving an interrupted ASCII
// character for the next iteration. Range failures consume the first two bytes.
static ULONG KexDecodeUtf8(PCUCHAR Source,ULONG Bytes,PULONG Consumed,PBOOLEAN Invalid) {
    ULONG Lead=Source[0],Code,Need,i,Byte;
    *Consumed=1;*Invalid=FALSE;
    if(Lead<0x80)return Lead;
    if(Lead>=0xc2 && Lead<=0xdf){Need=2;Code=Lead&0x1f;}
    else if(Lead>=0xe0 && Lead<=0xef){Need=3;Code=Lead&0xf;}
    else if(Lead>=0xf0 && Lead<=0xf4){Need=4;Code=Lead&7;}
    else {*Invalid=TRUE;return 0xfffd;}
    for(i=1;i<Need;++i) {
        if(i>=Bytes){*Invalid=TRUE;return 0xfffd;}
        Byte=Source[i];
        if((Byte&0xc0)!=0x80){*Invalid=TRUE;return 0xfffd;}
        ++*Consumed;
        if(i==1 && ((Lead==0xe0 && Byte<0xa0) || (Lead==0xed && Byte>=0xa0) ||
            (Lead==0xf0 && Byte<0x90) || (Lead==0xf4 && Byte>=0x90))) {
            *Invalid=TRUE;return 0xfffd;
        }
        Code=(Code<<6)|(Byte&0x3f);
    }
    return Code;
}

KEXAPI NTSTATUS NTAPI KexRtlUTF8ToUnicodeN(PWSTR Destination,ULONG Capacity,
    PULONG Actual,PCCH Source,ULONG Bytes) {
    static PVOID volatile Cache;
    KEX_UTF_CONVERT Native=KexNativeUtfConvert("RtlUTF8ToUnicodeN",&Cache);
    ULONG Used=0,Position=0,Consumed,Code,Units,i;WCHAR Encoded[2];BOOLEAN Invalid;
    NTSTATUS Status=STATUS_SUCCESS;
    if(Native)return Native(Destination,Capacity,Actual,Source,Bytes);
    if(!Source)return STATUS_INVALID_PARAMETER_4;
    if(!Destination && !Actual)return STATUS_INVALID_PARAMETER;
    while(Position<Bytes) {
        Code=KexDecodeUtf8((PCUCHAR)Source+Position,Bytes-Position,&Consumed,&Invalid);
        Position+=Consumed;
        if(Invalid)Status=STATUS_SOME_NOT_MAPPED;
        if(Code<0x10000){Encoded[0]=(WCHAR)Code;Units=1;}
        else {Code-=0x10000;Encoded[0]=(WCHAR)(0xd800+(Code>>10));Encoded[1]=(WCHAR)(0xdc00+(Code&0x3ff));Units=2;}
        for(i=0;i<Units;++i) {
            if(Destination && Capacity-Used<2){Status=STATUS_BUFFER_TOO_SMALL;goto Finished;}
            if(Used>MAXDWORD-2)return STATUS_INTEGER_OVERFLOW;
            if(Destination)Destination[Used/2]=Encoded[i];
            Used+=2;
        }
    }
Finished:
    if(Actual)*Actual=Used;
    return Status;
}

KEXAPI NTSTATUS NTAPI KexRtlUnicodeToUTF8N(PCHAR Destination,ULONG Capacity,
    PULONG Actual,PCWSTR Source,ULONG Bytes) {
    static PVOID volatile Cache;
    KEX_UTF_CONVERT Native=KexNativeUtfConvert("RtlUnicodeToUTF8N",&Cache);
    ULONG Used=0,Position=0,Code,Low,Length,i;UCHAR Encoded[4];
    NTSTATUS Status=STATUS_SUCCESS;
    if(Native)return Native(Destination,Capacity,Actual,Source,Bytes);
    if(!Source)return STATUS_INVALID_PARAMETER_4;
    if(!Destination && !Actual)return STATUS_INVALID_PARAMETER;
    if(Destination && (Bytes&1))return STATUS_INVALID_PARAMETER_5;
    Bytes/=2;
    while(Position<Bytes) {
        Code=Source[Position++];
        if(Code>=0xd800 && Code<=0xdbff && Position<Bytes &&
            (Low=Source[Position])>=0xdc00 && Low<=0xdfff) {
            ++Position;Code=0x10000+((Code-0xd800)<<10)+(Low-0xdc00);
        } else if(Code>=0xd800 && Code<=0xdfff) {
            Code=0xfffd;Status=STATUS_SOME_NOT_MAPPED;
        }
        if(Code<0x80){Encoded[0]=(UCHAR)Code;Length=1;}
        else if(Code<0x800){Encoded[0]=(UCHAR)(0xc0|(Code>>6));Encoded[1]=(UCHAR)(0x80|(Code&0x3f));Length=2;}
        else if(Code<0x10000){Encoded[0]=(UCHAR)(0xe0|(Code>>12));Encoded[1]=(UCHAR)(0x80|((Code>>6)&0x3f));Encoded[2]=(UCHAR)(0x80|(Code&0x3f));Length=3;}
        else {Encoded[0]=(UCHAR)(0xf0|(Code>>18));Encoded[1]=(UCHAR)(0x80|((Code>>12)&0x3f));Encoded[2]=(UCHAR)(0x80|((Code>>6)&0x3f));Encoded[3]=(UCHAR)(0x80|(Code&0x3f));Length=4;}
        if(Destination && Capacity-Used<Length){Status=STATUS_BUFFER_TOO_SMALL;break;}
        if(Used>MAXDWORD-Length)return STATUS_INTEGER_OVERFLOW;
        if(Destination)for(i=0;i<Length;++i)Destination[Used+i]=(CHAR)Encoded[i];
        Used+=Length;
    }
    // Native encode dereferences this even though the public annotation says
    // optional; validation failures above leave it untouched.
    *Actual=Used;
    return Status;
}
