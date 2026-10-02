#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef LONG (WINAPI *STATE_FN)(PCWSTR, PCWSTR, PCWSTR, ULONG, PWSTR, ULONG, PULONG);

int main(int argc, char **argv)
{
    FILE *out;
    HMODULE module;
    STATE_FN query;
    unsigned i, failures = 0;
    static const WCHAR path[] = L"\\Registry\\Machine\\Software\\VxKexProbe";
    if (argc != 3) return 2;
    out = fopen(argv[2], "w");
    if (!out) return 3;
    fprintf(out, "ProcessBits=%u\n", (unsigned)(sizeof(void *) * 8));
    module = LoadLibraryA(argv[1]);
    if (!module) { fprintf(out, "LoadLibraryError=%lu\n", GetLastError()); fclose(out); return 4; }
    query = (STATE_FN)GetProcAddress(module, "RtlGetPersistedStateLocation");
    if (!query) { fprintf(out, "ExportMissing\n"); fclose(out); return 5; }
    for (i = 0; i < 16; ++i) {
        struct { DWORD before; WCHAR text[128]; DWORD after; } buffer;
        DWORD size = sizeof(path), required = 0xabcdef01;
        ULONG type = 0;
        PCWSTR def = path, custom = NULL;
        PWSTR target = buffer.text;
        PULONG requiredOut = &required;
        LONG status;
        DWORD exception = 0;
        unsigned j;
        buffer.before = buffer.after = 0x12345678;
        for (j = 0; j < 128; ++j) buffer.text[j] = 0xcccc;
        switch (i) {
        case 1: type = 1; break;
        case 2: type = 2; break;
        case 3: type = 0xffffffff; break;
        case 4: size = 0; target = NULL; break;
        case 5: size = sizeof(path) - 2; break;
        case 6: size = sizeof(path) - 1; break;
        case 7: size = sizeof(path) + 1; break;
        case 8: size = sizeof(buffer.text); break;
        case 9: requiredOut = NULL; break;
        case 10: def = L""; size = 2; break;
        case 11: def = NULL; break;
        case 12: def = NULL; type = 1; break;
        case 13: custom = L"Other"; break;
        case 14: def = L"C:\\VxKexProbe\\\x65e5\x672c"; type = 1; size = sizeof(buffer.text); break;
        case 15: target = NULL; break;
        }
        __try {
            status = query(L"VxKex-Probe-{00B55D1B-77C8-45D4-BC57-D0B453512211}",
                custom, def, type, target, size, requiredOut);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            exception = GetExceptionCode();
            status = (LONG)exception;
        }
        fprintf(out, "Case=%u Status=%08lx Required=%lu Exception=%08lx Text=",
            i, (ULONG)status, required, exception);
        if (status == 0 && target) {
            if (!def || wcscmp(target, def) != 0) ++failures;
            for (j = 0; j < 128 && target[j]; ++j) fprintf(out, "%04x", target[j]);
        } else {
            for (j = 0; j < 128; ++j) if (buffer.text[j] != 0xcccc) { ++failures; break; }
        }
        if (buffer.before != 0x12345678 || buffer.after != 0x12345678) ++failures;
        fputc('\n', out);
    }
    fprintf(out, "PersistedStateCases=16 Failures=%u\n", failures);
    fprintf(out, "Result=%s\n", failures ? "FAIL" : "PASS");
    fclose(out);
    return failures ? 1 : 0;
}
