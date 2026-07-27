import os
import subprocess
import sys

# Define directories
source_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex_Vista"
kexcfg_dir = os.path.join(source_dir, "KexCfg")
release_dir = os.path.join(source_dir, "x64", "Release", "KexCfg")

# Ensure Release directory exists
os.makedirs(release_dir, exist_ok=True)

# Define VS2010 variables
vs_dir = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0"
vc_dir = os.path.join(vs_dir, "VC")
sdk_dir = r"C:\Program Files\Microsoft SDKs\Windows\v7.1"

# Environment variables
env = os.environ.copy()
env["INCLUDE"] = f"{os.path.join(vc_dir, 'include')};{os.path.join(sdk_dir, 'include')};{os.path.join(source_dir, '00-Common-Headers')}"
env["LIB"] = f"{os.path.join(vc_dir, 'lib', 'amd64')};{os.path.join(sdk_dir, 'lib', 'x64')};{os.path.join(source_dir, 'x64', 'Release', 'KxBase')};{os.path.join(source_dir, 'x64', 'Release', 'KxCfgHlp')};{os.path.join(source_dir, 'x64', 'Release', 'KexW32ML')};{os.path.join(source_dir, 'x64', 'Release', 'KexPathCch')};{os.path.join(source_dir, '00-Import-Libraries')}"
env["PATH"] = f"{os.path.join(vc_dir, 'bin', 'amd64')};{os.path.join(vs_dir, 'Common7', 'IDE')};{env.get('PATH', '')}"

# Compilation flags
cl_flags = [
    "cl.exe",
    "/c",
    "/O2",
    "/W3",
    "/GS-",
    "/D_WIN64",
    "/D_AMD64_",
    "/DNDEBUG",
    "/D_UNICODE",
    "/DUNICODE",
    "/Fd" + os.path.join(release_dir, "vc100.pdb"),
    "/Fo" + os.path.join(release_dir, "main.obj"),
    os.path.join(kexcfg_dir, "main.c")
]

print("Compiling KexCfg main.c...")
try:
    subprocess.check_call(cl_flags, env=env, shell=True)
except subprocess.CalledProcessError as e:
    print(f"Compilation failed with error {e.returncode}")
    sys.exit(1)

# Linker flags
link_flags = [
    "link.exe",
    "/OUT:" + os.path.join(release_dir, "KexCfg.exe"),
    "/MACHINE:X64",
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF",
    "/OPT:ICF",
    "/NODEFAULTLIB",
    "/ENTRY:wWinMain",
    os.path.join(release_dir, "main.obj"),
    "KxBase.lib",
    "KxCfgHlp.lib",
    "KexW32ML.lib",
    "KexPathCch.lib",
    "kernel32.lib",
    "user32.lib",
    "shell32.lib",
    "advapi32.lib",
    "ole32.lib",
    "oleaut32.lib",
    "uuid.lib",
    "taskschd.lib",
    "shlwapi.lib",
    "msvcrt_x64.lib",
    "ntdll_x64.lib"
]

print("Linking KexCfg.exe...")
try:
    subprocess.check_call(link_flags, env=env, shell=True)
    print(f"Successfully built KexCfg.exe at {os.path.join(release_dir, 'KexCfg.exe')}")
except subprocess.CalledProcessError as e:
    print(f"Linking failed with error {e.returncode}")
    sys.exit(1)
