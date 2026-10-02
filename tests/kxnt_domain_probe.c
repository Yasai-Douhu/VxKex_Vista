#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } TEST_STRING;
typedef LONG (WINAPI *DOMAIN_FN)(TEST_STRING *, const TEST_STRING *, BOOLEAN);
typedef VOID (WINAPI *FREE_FN)(TEST_STRING *);

static const struct { const WCHAR *input; BOOLEAN strict; } cases[] = {
    {L"EXAMPLE.COM", TRUE}, {L"example.com.", TRUE},
    {L"xn--bcher-kva.de", TRUE}, {L"b\x00fc" L"cher.de", TRUE},
    {L"\x00dc" L"BER.example", TRUE}, {L"\x4f8b\x3048.\x30c6\x30b9\x30c8", TRUE},
    {L"192.0.2.1", TRUE}, {L"127.1", FALSE}, {L"127.1", TRUE},
    {L"0x7f000001", FALSE}, {L"010.0.0.1", FALSE},
    {L"2001:0DB8:0:0:0:0:0:1", TRUE}, {L"::1", TRUE},
    {L"::ffff:192.0.2.1", TRUE}, {L"fe80::1%2", TRUE},
    {L"[::1]", TRUE}, {L"[::1]:80", TRUE}, {L"example.com:80", TRUE},
    {L"192.0.2.1:80", TRUE}, {L"", TRUE}, {L"bad..example", TRUE},
    {L"a b.example", TRUE}, {L"a_b.example", TRUE}, {L"\xd800", TRUE},
    {L"\xdc00", TRUE}
};

int main(int argc, char **argv)
{
    HMODULE m;
    DOMAIN_FN canonicalize;
    FREE_FN release;
    FILE *out;
    unsigned i;
    unsigned failures = 0;
    if (argc != 3) return 2;
    out = fopen(argv[2], "w");
    if (!out) return 3;
    fprintf(out, "ProcessBits=%u\n", (unsigned)(sizeof(void *) * 8));
    m = LoadLibraryA(argv[1]);
    if (!m) { fprintf(out, "LoadLibraryError=%lu\n", GetLastError()); fclose(out); return 4; }
    canonicalize = (DOMAIN_FN)GetProcAddress(m, "RtlCanonicalizeDomainName");
    release = (FREE_FN)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlFreeUnicodeString");
    if (!canonicalize || !release) { fprintf(out, "ExportMissing\n"); fclose(out); return 5; }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TEST_STRING src, dst = {0};
        LONG status;
        unsigned j;
        src.Buffer = (PWSTR)cases[i].input;
        src.Length = (USHORT)(wcslen(src.Buffer) * sizeof(WCHAR));
        src.MaximumLength = src.Length;
        status = canonicalize(&dst, &src, cases[i].strict);
        fprintf(out, "Case=%u Status=%08lx Length=%u Text=", i, (ULONG)status, dst.Length);
        if (status >= 0) {
            if (!dst.Buffer || dst.MaximumLength <= dst.Length ||
                (dst.Length % sizeof(WCHAR)) || dst.Buffer[dst.Length / sizeof(WCHAR)] != 0) {
                fprintf(out, "INVALID_OUTPUT");
                ++failures;
            }
            for (j = 0; j < dst.Length / sizeof(WCHAR); ++j) fprintf(out, "%04x", (unsigned)dst.Buffer[j]);
            release(&dst);
        }
        fputc('\n', out);
    }
    // Source ends at a guard page without a terminator. Length is in bytes;
    // accessing even one WCHAR beyond it must fail the test.
    {
        SYSTEM_INFO system;
        BYTE *pages;
        DWORD old;
        TEST_STRING src, dst = {0};
        LONG status;
        GetSystemInfo(&system);
        pages = (BYTE *)VirtualAlloc(NULL, system.dwPageSize * 2,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pages) { fclose(out); return 6; }
        if (!VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &old)) {
            VirtualFree(pages, 0, MEM_RELEASE);
            fclose(out);
            return 7;
        }
        src.Length = 22;
        src.MaximumLength = 22;
        src.Buffer = (PWSTR)(pages + system.dwPageSize - src.Length);
        memcpy(src.Buffer, L"EXAMPLE.COM", src.Length);
        __try { status = canonicalize(&dst, &src, TRUE); }
        __except(EXCEPTION_EXECUTE_HANDLER) { status = (LONG)0xc0000005; }
        if (status != 0 || dst.Length != 22 ||
            !dst.Buffer || wmemcmp(dst.Buffer, L"example.com", 11) != 0) ++failures;
        if (status >= 0) release(&dst);
        fprintf(out, "GuardPageStatus=%08lx\n", (ULONG)status);
        VirtualFree(pages, 0, MEM_RELEASE);
    }
    {
        WCHAR longName[257];
        TEST_STRING src, dst = {0};
        LONG status;
        for (i = 0; i < 256; ++i) longName[i] = L'a';
        longName[256] = 0;
        src.Buffer = longName;
        src.Length = 512;
        src.MaximumLength = 514;
        status = canonicalize(&dst, &src, TRUE);
        if (status != (LONG)0xc0000716) ++failures;
        if (status >= 0) release(&dst);
        fprintf(out, "LongNameStatus=%08lx\n", (ULONG)status);
    }
    for (i = 0; i < 2000; ++i) {
        TEST_STRING src, dst = {0};
        LONG status;
        src.Buffer = L"EXAMPLE.COM";
        src.Length = src.MaximumLength = 22;
        status = canonicalize(&dst, &src, TRUE);
        if (status != 0 || dst.Length != 22) ++failures;
        if (status >= 0) release(&dst);
    }
    if (!HeapValidate(GetProcessHeap(), 0, NULL)) ++failures;
    fprintf(out, "AllocationFreeCycles=2000 Failures=%u\n", failures);
    fprintf(out, "Result=%s\n", failures ? "FAIL" : "PASS");
    fclose(out);
    return failures ? 1 : 0;
}
