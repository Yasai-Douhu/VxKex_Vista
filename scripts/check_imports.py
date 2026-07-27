import os
import subprocess
import sys

def get_imports_using_dumpbin(dll_path):
    dumpbin_path = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\dumpbin.exe"
    try:
        output = subprocess.check_output([dumpbin_path, "/IMPORTS", dll_path], stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running dumpbin on {dll_path}")
        return {}
    
    imports = {}
    current_dll = None
    lines = output.split('\n')
    for i, line in enumerate(lines):
        line = line.strip()
        if line.endswith(".dll") and " " not in line:
            current_dll = line.lower()
            imports[current_dll] = set()
            continue
        
        # Parse import functions
        if current_dll and line:
            parts = line.split()
            if len(parts) >= 2 and parts[0].isalnum() and len(parts[0]) <= 8:
                # E.g. "     1EE    LdrLoadDll" or "       1    Ordinal     1"
                # Sometimes no hint: "          LdrLoadDll"
                pass
            if len(parts) >= 1 and current_dll in imports:
                func_name = parts[-1]
                if not func_name.startswith("Ordinal") and not func_name.endswith(".dll") and func_name.isidentifier():
                    imports[current_dll].add(func_name)
    return imports

def get_exports_using_dumpbin(dll_path):
    dumpbin_path = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\dumpbin.exe"
    try:
        output = subprocess.check_output([dumpbin_path, "/EXPORTS", dll_path], stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running dumpbin on {dll_path}")
        return set()
    
    exports = set()
    lines = output.split('\n')
    parsing_exports = False
    for line in lines:
        if "ordinal hint RVA      name" in line:
            parsing_exports = True
            continue
        if parsing_exports and not line.strip():
            # End of exports or empty line, we just keep parsing since there's "Summary" at the end
            pass
        if parsing_exports and line.strip():
            if "Summary" in line:
                break
            parts = line.strip().split()
            # E.g. "      1    0 00012345 FunctionName"
            # Some might be forwarded: "      2    1          FunctionName (forwarded to dll.function)"
            if len(parts) >= 4 and parts[0].isdigit():
                # Is there a name?
                # Sometimes there's no name, just ordinal, e.g. "      3    2 00012345 [NONAME]"
                name = parts[3]
                if name != "[NONAME]":
                    exports.add(name)
            elif len(parts) >= 3 and parts[0].isdigit() and parts[1].isdigit() and not parts[2].startswith("00"):
                # Forwarded without RVA? usually: ordinal hint name (forwarded...)
                name = parts[2]
                exports.add(name)
    return exports

vista_system_files = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\SystemFiles\vista"
built_dlls_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex_Vista\VistaDLLs"

# Precompute exports of vista system files
vista_exports = {}
for root, _, files in os.walk(vista_system_files):
    for f in files:
        if f.lower().endswith(".dll"):
            dll_path = os.path.join(root, f)
            print(f"Parsing exports for {f}...")
            vista_exports[f.lower()] = get_exports_using_dumpbin(dll_path)

# Check imports of built DLLs
missing = []
for root, _, files in os.walk(built_dlls_dir):
    for f in files:
        if f.lower().endswith(".dll"):
            dll_path = os.path.join(root, f)
            print(f"Checking {f}...")
            imports = get_imports_using_dumpbin(dll_path)
            for imp_dll, funcs in imports.items():
                if imp_dll in vista_exports:
                    for func in funcs:
                        if func not in vista_exports[imp_dll]:
                            missing.append((f, imp_dll, func))
                else:
                    # Ignore DLLs we don't have in SystemFiles
                    pass

print("\n--- MISSING APIs in VISTA ---")
for dll, imp_dll, func in missing:
    print(f"[{dll}] imports {func} from {imp_dll} BUT it is MISSING in Vista!")
if not missing:
    print("All imported APIs are present in Vista SystemFiles!")

