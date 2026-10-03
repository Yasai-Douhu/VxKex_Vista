#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>

// Map an owned test executable without running it, then resolve its imports
// through the real loader. Optional argument redirects only ntdll imports to
// the specified scratch KxNt DLL; it does not patch the executable or registry.
int main(int argc,char **argv)
{
    FILE *out; HMODULE replacement=NULL,module; BYTE *base;
    HANDLE file,mapping;
    IMAGE_NT_HEADERS *nt; IMAGE_IMPORT_DESCRIPTOR *descriptor;
    IMAGE_THUNK_DATA *thunk; unsigned total=0,missing=0; char path[MAX_PATH];
    if(argc<3 || argc>4)return 2;
    out=fopen(argv[2],"w");if(!out)return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc==4){replacement=LoadLibraryA(argv[3]);if(!replacement){fprintf(out,"ReplacementLoadError=%lu\n",GetLastError());fclose(out);return 4;}}
    // Direct SEC_IMAGE mapping avoids loader notifications which could rewrite
    // imports even with DONT_RESOLVE_DLL_REFERENCES. Keep original import names.
    file=CreateFileA(argv[1],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){fprintf(out,"FileError=%lu\n",GetLastError());fclose(out);return 5;}
    mapping=CreateFileMappingA(file,NULL,PAGE_READONLY|SEC_IMAGE,0,0,NULL);
    if(!mapping){fprintf(out,"MappingError=%lu\n",GetLastError());fclose(out);return 5;}
    base=(BYTE*)MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);
    if(!base){fprintf(out,"ImageMapError=%lu\n",GetLastError());fclose(out);return 5;}
    nt=(IMAGE_NT_HEADERS*)(base+((IMAGE_DOS_HEADER*)base)->e_lfanew);
    descriptor=(IMAGE_IMPORT_DESCRIPTOR*)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    fprintf(out,"ProcessBits=%u RedirectNtdll=%d\n",(unsigned)(sizeof(void*)*8),replacement!=NULL);
    for(;descriptor->Name;descriptor++) {
        const char *library=(const char*)(base+descriptor->Name);
        module=replacement && _stricmp(library,"ntdll.dll")==0?replacement:LoadLibraryA(library);
        if(!module){fprintf(out,"Module=%s Error=%lu\n",library,GetLastError());missing++;continue;}
        GetModuleFileNameA(module,path,MAX_PATH);fprintf(out,"Module=%s LoadedPath=%s\n",library,path);
        thunk=(IMAGE_THUNK_DATA*)(base+(descriptor->OriginalFirstThunk?descriptor->OriginalFirstThunk:descriptor->FirstThunk));
        for(;thunk->u1.AddressOfData;thunk++) {
            FARPROC resolved; const char *name; char ordinal[32];
            if(IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) {
                sprintf(ordinal,"#%u",(unsigned)IMAGE_ORDINAL(thunk->u1.Ordinal));name=ordinal;
                resolved=GetProcAddress(module,(LPCSTR)IMAGE_ORDINAL(thunk->u1.Ordinal));
            } else {
                name=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+thunk->u1.AddressOfData))->Name;
                resolved=GetProcAddress(module,name);
            }
            fprintf(out,"Import=%s!%s Resolved=%d\n",library,name,resolved!=NULL);
            total++;if(!resolved)missing++;
        }
    }
    module=GetModuleHandleA("KexDll.dll");if(module){GetModuleFileNameA(module,path,MAX_PATH);fprintf(out,"KexDllPath=%s\n",path);}
    fprintf(out,"TotalImports=%u Missing=%u Result=%s\n",total,missing,missing?"UNRESOLVED":"RESOLVED");
    UnmapViewOfFile(base);CloseHandle(mapping);CloseHandle(file);
    fclose(out);return missing?1:0;
}
