#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>

typedef BOOLEAN (WINAPI *RTL_FEATURE)(ULONG);

int main(int argc, char **argv)
{
    HMODULE module;
    RTL_FEATURE feature;
    ULONG i;
    unsigned failures = 0;
    FILE *out = stdout;
    const ULONG invalid[] = {64, 65, 255, 0x7fffffffUL, 0xffffffffUL};
    if (argc != 3) return 2;
    out = fopen(argv[2], "w");
    if (!out) return 3;
    fprintf(out, "ProcessBits=%u\n", (unsigned)(sizeof(void *) * 8));
    fprintf(out, "NativeRtlIsProcessorFeaturePresent=%s\n",
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
        "RtlIsProcessorFeaturePresent") ? "PRESENT" : "ABSENT");
    SetLastError(0);
    module = LoadLibraryA(argv[1]);
    if (!module) {
        fprintf(out, "LoadLibraryError=%lu\n", GetLastError());
        fclose(out);
        return 4;
    }
    feature = (RTL_FEATURE)GetProcAddress(module, "RtlIsProcessorFeaturePresent");
    if (!feature) {
        fprintf(out, "GetProcAddressError=%lu\n", GetLastError());
        fclose(out);
        return 5;
    }
    // Native RTL (when available) supplies the preferred behavioral oracle.
    // On NT 6.0 the documented Win32 API supplies the OS feature policy.
    for (i = 0; i < 64; ++i) {
        BOOLEAN actual;
        RTL_FEATURE nativeFeature = (RTL_FEATURE)GetProcAddress(
            GetModuleHandleW(L"ntdll.dll"), "RtlIsProcessorFeaturePresent");
        BOOL expected = IsProcessorFeaturePresent(i);
        if (nativeFeature) {
            expected = nativeFeature(i);
        }
        SetLastError(0x12345678);
        actual = feature(i);
        if (!!actual != !!expected || GetLastError() != 0x12345678) {
            fprintf(out, "Feature=%lu Actual=%u Expected=%u LastError=%lu\n",
                i, (unsigned)actual, (unsigned)expected, GetLastError());
            ++failures;
        }
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        if (feature(invalid[i]) != FALSE) {
            fprintf(out, "InvalidFeature=%lu did not return FALSE\n", invalid[i]);
            ++failures;
        }
    }
    fprintf(out, "ValidFeatureChecks=64 InvalidFeatureChecks=5 Failures=%u\n", failures);
    fprintf(out, "Result=%s\n", failures ? "FAIL" : "PASS");
    fclose(out);
    return failures ? 1 : 0;
}
