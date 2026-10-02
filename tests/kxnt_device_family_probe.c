#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <intrin.h>

typedef VOID (WINAPI *FAMILY_FN)(ULONGLONG *, ULONG *, ULONG *);
typedef LONG (WINAPI *VERSION_FN)(OSVERSIONINFOEXW *);

int main(int argc, char **argv)
{
    HMODULE module;
    FAMILY_FN query;
    VERSION_FN versionFn;
    OSVERSIONINFOEXW version = {0};
    unsigned mask, failures = 0;
    ULONGLONG expectedVersion;
    ULONG expectedFamily;
    FILE *out;
    if (argc != 3) return 2;
    out = fopen(argv[2], "w");
    if (!out) return 3;
    fprintf(out, "ProcessBits=%u\n", (unsigned)(sizeof(void *) * 8));
    fprintf(out, "NativeDeviceFamily=%s\n", GetProcAddress(
        GetModuleHandleW(L"ntdll.dll"), "RtlGetDeviceFamilyInfoEnum") ? "PRESENT" : "ABSENT");
    module = LoadLibraryA(argv[1]);
    if (!module) { fprintf(out, "LoadLibraryError=%lu\n", GetLastError()); fclose(out); return 4; }
    query = (FAMILY_FN)GetProcAddress(module, "RtlGetDeviceFamilyInfoEnum");
    versionFn = (VERSION_FN)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (!query || !versionFn) { fprintf(out, "ExportMissing\n"); fclose(out); return 5; }
    version.dwOSVersionInfoSize = sizeof(version);
    if (versionFn(&version) != 0) { fclose(out); return 6; }
    expectedVersion = ((ULONGLONG)version.dwMajorVersion << 48) |
        ((ULONGLONG)version.dwMinorVersion << 32) | ((ULONGLONG)version.dwBuildNumber << 16);
    expectedFamily = version.wProductType == VER_NT_WORKSTATION ? 3 : 9;
    fprintf(out, "ReportedVersion=%lu.%lu.%lu ProductType=%u\n",
        version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber, version.wProductType);
    for (mask = 0; mask < 8; ++mask) {
        struct { ULONG before; ULONGLONG value; ULONG after; } uap;
        struct { ULONG before, value, after; } family, form;
        uap.before = uap.after = family.before = family.after = form.before = form.after = 0xabcde123;
        uap.value = 0x1122334455667788ULL;
        family.value = form.value = 0x11223344;
        SetLastError(0x13572468);
        query(mask & 1 ? &uap.value : NULL, mask & 2 ? &family.value : NULL,
            mask & 4 ? &form.value : NULL);
        if (uap.value != (mask & 1 ? expectedVersion : 0x1122334455667788ULL) ||
            family.value != (mask & 2 ? expectedFamily : 0x11223344) ||
            form.value != (mask & 4 ? 3UL : 0x11223344) ||
            uap.before != 0xabcde123 || uap.after != 0xabcde123 ||
            family.before != 0xabcde123 || family.after != 0xabcde123 ||
            form.before != 0xabcde123 || form.after != 0xabcde123 ||
            GetLastError() != 0x13572468) {
            ++failures;
            fprintf(out, "Mask=%u FAILED UAP=%I64x Family=%lu Form=%lu\n", mask,
                uap.value, family.value, form.value);
        }
    }
    // Change only this probe's PEB version fields, and restore them even if
    // the compatibility function raises an exception. Verify the offsets
    // against RtlGetVersion before writing anything.
    {
        BYTE *peb;
        ULONG *major, *minor;
        USHORT *build;
        unsigned i;
        const ULONG targets[][3] = {{10,0,19045},{10,0,26100},{6,1,7601}};
#ifdef _WIN64
        peb = (BYTE *)__readgsqword(0x60);
        major = (ULONG *)(peb + 0x118);
        minor = (ULONG *)(peb + 0x11c);
        build = (USHORT *)(peb + 0x120);
#else
        peb = (BYTE *)__readfsdword(0x30);
        major = (ULONG *)(peb + 0xa4);
        minor = (ULONG *)(peb + 0xa8);
        build = (USHORT *)(peb + 0xac);
#endif
        if (*major != version.dwMajorVersion || *minor != version.dwMinorVersion ||
            *build != version.dwBuildNumber) {
            ++failures;
            fprintf(out, "PEBLayoutVerification=FAIL\n");
        } else {
            ULONG savedMajor = *major, savedMinor = *minor;
            USHORT savedBuild = *build;
            __try {
                for (i = 0; i < 3; ++i) {
                    ULONGLONG value = 0;
                    ULONG family = 0, form = 0;
                    *major = targets[i][0];
                    *minor = targets[i][1];
                    *build = (USHORT)targets[i][2];
                    query(&value, &family, &form);
                    if ((value >> 48) != targets[i][0] ||
                        ((value >> 32) & 0xffff) != targets[i][1] ||
                        ((value >> 16) & 0xffff) != targets[i][2] ||
                        (value & 0xffff) != 0 || family != expectedFamily || form != 3) ++failures;
                }
            } __finally {
                *major = savedMajor;
                *minor = savedMinor;
                *build = savedBuild;
            }
        }
        fprintf(out, "SpoofedVersionCases=3\n");
    }
    fprintf(out, "OptionalOutputCombinations=8 Failures=%u\n", failures);
    fprintf(out, "Result=%s\n", failures ? "FAIL" : "PASS");
    fclose(out);
    return failures ? 1 : 0;
}
