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

base_dir = r"C:\Users\YamaR\Desktop\VxKex_Vista"
kxbase_dir = os.path.join(base_dir, "KxBase")
out_dir = os.path.join(base_dir, "x64", "Release", "KxBase")

if not os.path.exists(out_dir):
    os.makedirs(out_dir)

c_files = glob.glob(os.path.join(kxbase_dir, "*.c"))
obj_files = []

for c_file in c_files:
    obj_name = os.path.basename(c_file).replace(".c", ".obj")
    obj_file = os.path.join(out_dir, obj_name)
    obj_files.append(obj_file)
    
    cmd = [
        "cl.exe", "/nologo", "/c", "/O1", "/Os", "/Oy", "/GL", "/Gy", "/Gz", "/MD", "/Zi", "/W3", "/TC", "/GS-",
        "/D", "WIN32", "/D", "NDEBUG", "/D", "_WINDOWS", "/D", "_USRDLL", "/D", "KXBASE_EXPORTS", 
        "/D", "_WIN32_WINNT=0x0600", "/D", "WINVER=0x0600", "/D", "PSAPI_VERSION=1",
        "/I", os.path.join(base_dir, "00-Common-Headers"),
        "/Fo" + obj_file, c_file
    ]
    print(f"Compiling {os.path.basename(c_file)}...")
    subprocess.check_call(cmd)

import_libs_dir = r"C:\Users\YamaR\Desktop\VxKex_Vista\00-Import-Libraries"
prebuilt_libs_dir = import_libs_dir

link_cmd = [
    "link.exe", "/NOLOGO", "/DLL",
    f"/OUT:{os.path.join(out_dir, 'KxBase.dll')}",
    f"/IMPLIB:{os.path.join(out_dir, 'KxBase.lib')}",
    "/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF", "/MACHINE:X64",
    "/ENTRY:DllMain", "/LTCG",
    f"/DEF:{os.path.join(kxbase_dir, 'kxbase.def')}",
    f"/LIBPATH:{import_libs_dir}",
    f"/LIBPATH:{os.path.join(base_dir, 'VistaDLLs')}",
    "KexW32ML.lib"
] + obj_files + [
    os.path.join(prebuilt_libs_dir, "KexDll.lib"),
    os.path.join(prebuilt_libs_dir, "KexPathCch.lib"),
    os.path.join(prebuilt_libs_dir, "KexSmp.lib"),
    os.path.join(prebuilt_libs_dir, "KexMls.lib"),
    os.path.join(import_libs_dir, "ntdll_x64.lib"),
    os.path.join(import_libs_dir, "msvcrt_x64.lib"),
    os.path.join(import_libs_dir, "kernel32_x64.lib"),
    os.path.join(import_libs_dir, "user32_x64.lib"),
    "advapi32.lib", "shlwapi.lib", "psapi.lib", "version.lib"
]

print("Linking...")
subprocess.check_call(link_cmd)
print("Done.")
