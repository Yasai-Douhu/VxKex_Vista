import os
import subprocess
import sys

# ディレクトリ定義
source_dir = r"C:\Users\YamaR\Desktop\AI_Datas\VxKex_Vista\VxKex_Vista"
kexshlex_dir = os.path.join(source_dir, "KexShlEx")
release_dir = os.path.join(source_dir, "x64", "Release", "KexShlEx")

# ビルド出力ディレクトリを確保
os.makedirs(release_dir, exist_ok=True)

# VS2010 パス定義
vs_dir = r"C:\Program Files (x86)\Microsoft Visual Studio 10.0"
vc_dir = os.path.join(vs_dir, "VC")
sdk_dir = r"C:\Program Files\Microsoft SDKs\Windows\v7.1"

# 環境変数設定
env = os.environ.copy()
env["INCLUDE"] = (
    f"{os.path.join(vc_dir, 'include')};"
    f"{os.path.join(sdk_dir, 'include')};"
    f"{os.path.join(source_dir, '00-Common-Headers')}"
)
env["LIB"] = (
    f"{os.path.join(vc_dir, 'lib', 'amd64')};"
    f"{os.path.join(sdk_dir, 'lib', 'x64')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexShlEx')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KxBase')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexW32ML')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexPathCch')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KxCfgHlp')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexGui')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexMLS')};"
    f"{os.path.join(source_dir, 'x64', 'Release', 'KexSmp')};"
    f"{os.path.join(source_dir, '00-Import-Libraries')}"
)
env["PATH"] = f"{os.path.join(vc_dir, 'bin', 'amd64')};{os.path.join(vs_dir, 'Common7', 'IDE')};{env.get('PATH', '')}"

# コンパイル対象ファイル
source_files = [
    "ckxshlex.c",
    "clsfctry.c",
    "dllmain.c",
    "gui.c",
    "lnkfile.c",
]

# オブジェクトファイル出力先
obj_files = []

for src in source_files:
    src_path = os.path.join(kexshlex_dir, src)
    obj_path = os.path.join(release_dir, src.replace(".c", ".obj"))
    obj_files.append(obj_path)

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
        "/LD",
        "/Fd" + os.path.join(release_dir, "vc100.pdb"),
        "/Fo" + obj_path,
        src_path,
    ]

    print(f"Compiling {src}...")
    try:
        subprocess.check_call(cl_flags, env=env, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Compilation failed: {e.returncode}")
        sys.exit(1)

# リソースコンパイル
rc_path = os.path.join(kexshlex_dir, "KexShlEx.rc")
res_path = os.path.join(release_dir, "KexShlEx.res")
rc_flags = [
    "rc.exe",
    "/nologo",
    f"/I{os.path.join(kexshlex_dir)}",
    f"/I{os.path.join(source_dir, '00-Common-Headers')}",
    f"/I{os.path.join(sdk_dir, 'include')}",
    "/fo" + res_path,
    rc_path,
]

# rcコンパイラをSDKのbinから探す
rc_bin = os.path.join(sdk_dir, "Bin", "x64", "RC.Exe")
if not os.path.exists(rc_bin):
    rc_bin = os.path.join(sdk_dir, "Bin", "RC.Exe")
rc_flags[0] = rc_bin

print("Compiling resources...")
try:
    subprocess.check_call(rc_flags, env=env, shell=True)
except subprocess.CalledProcessError as e:
    print(f"Resource compilation failed: {e.returncode}")
    # リソースコンパイルが失敗してもリンクは続行（resなしで試みる）
    res_path = None

# リンク
link_flags = [
    "link.exe",
    "/OUT:" + os.path.join(release_dir, "KexShlEx.dll"),
    "/DLL",
    "/MACHINE:X64",
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF",
    "/OPT:ICF",
    "/NODEFAULTLIB",
    "/DEF:" + os.path.join(kexshlex_dir, "KexShlEx.def"),
] + obj_files

if res_path:
    link_flags.append(res_path)

link_flags += [
    "KxBase.lib",
    "KxCfgHlp.lib",
    "KexW32ML.lib",
    "KexGui.lib",
    "KexMls.lib",
    "KexSmp.lib",
    "kernel32.lib",
    "user32.lib",
    "shell32.lib",
    "advapi32.lib",
    "ole32.lib",
    "oleaut32.lib",
    "uuid.lib",
    "taskschd.lib",
    "shlwapi.lib",
    "comctl32.lib",
    "msvcrt_x64.lib",
    "ntdll_x64.lib",
]

print("Linking KexShlEx.dll...")
try:
    subprocess.check_call(link_flags, env=env, shell=True)
    print(f"Successfully built KexShlEx.dll at {os.path.join(release_dir, 'KexShlEx.dll')}")
except subprocess.CalledProcessError as e:
    print(f"Linking failed: {e.returncode}")
    sys.exit(1)
