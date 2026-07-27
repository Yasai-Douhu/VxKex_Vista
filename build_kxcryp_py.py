import os
import subprocess
import glob

vs_bin = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64"
sdk_bin = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin"
sdk_inc = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include"
vs_inc = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include"
sdk_lib = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib\x64"
vs_lib = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64"

os.environ["PATH"] = f"{vs_bin};{sdk_bin};{os.environ.get('PATH', '')}"
os.environ["INCLUDE"] = f"{sdk_inc};{vs_inc}"
os.environ["LIB"] = f"{sdk_lib};{vs_lib}"

base_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex_Vista"
kxcryp_dir = os.path.join(base_dir, "KxCryp")
out_dir = os.path.join(base_dir, "x64", "Release", "KxCryp")

import_libs_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex-NEXT\00-Import Libraries"
prebuilt_libs_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex-NEXT\x64\Release"

if not os.path.exists(out_dir):
    os.makedirs(out_dir)

c_files = glob.glob(os.path.join(kxcryp_dir, "*.c"))
obj_files = []

for c_file in c_files:
    obj_name = os.path.basename(c_file).replace(".c", ".obj")
    obj_file = os.path.join(out_dir, obj_name)
    obj_files.append(obj_file)
    
    cmd = [
        "cl.exe", "/nologo", "/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-",
        "/D", "WIN32", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "_USRDLL", "/D", "KXCRYP_EXPORTS",
        "/D", "_WIN32_WINNT=0x0600", "/D", "WINVER=0x0600",
        "/I", os.path.join(base_dir, "00-Common-Headers"),
        "/Fo" + obj_file, c_file
    ]
    print(f"Compiling {os.path.basename(c_file)}...")
    result = subprocess.run(cmd, capture_output=False)
    if result.returncode != 0:
        print(f"ERROR: Failed to compile {c_file}")
        exit(1)

dll_path = os.path.join(out_dir, "KxCryp.dll")
lib_path = os.path.join(out_dir, "KxCryp.lib")
def_path = os.path.join(kxcryp_dir, "kxcryp.def")

link_cmd = [
    "link.exe", "/NOLOGO", "/DLL",
    f"/OUT:{dll_path}",
    f"/IMPLIB:{lib_path}",
    "/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", "/MACHINE:X64",
    "/ENTRY:DllMain", "/LTCG",
    f"/DEF:{def_path}",
    f"/LIBPATH:{import_libs_dir}",
    f"/LIBPATH:{os.path.join(base_dir, 'VistaDLLs')}",
    "KexW32ML.lib"
] + obj_files + [
    os.path.join(prebuilt_libs_dir, "KexDll", "KexDll.lib"),
    os.path.join(import_libs_dir, "ntdll_x64.lib"),
    os.path.join(import_libs_dir, "msvcrt_x64.lib"),
    os.path.join(import_libs_dir, "kernel32_x64.lib"),
    os.path.join(import_libs_dir, "user32_x64.lib"),
    "advapi32.lib", "shlwapi.lib", "secur32.lib", "bcrypt.lib"
]

print("Linking KxCryp.dll...")
result = subprocess.run(link_cmd, capture_output=False)
if result.returncode != 0:
    print("ERROR: Linking failed")
    exit(1)
print("Done.")
