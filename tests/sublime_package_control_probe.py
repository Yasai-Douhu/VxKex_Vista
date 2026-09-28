"""Run only in an isolated Sublime 4213 package using Python 3.14.

Checks sockets and installs the signed Package Control package in that profile.
"""
import ctypes
import hashlib
import json
import os
import socket
import ssl
import sys
import traceback
import urllib.request
import sublime
import sublime_plugin


def plugin_loaded():
    sublime.set_timeout_async(run_probe)


def require(condition):
    if not condition:
        raise RuntimeError('Probe check failed')


def run_probe():
    result = {"pid": os.getpid(), "sockets": [], "profile": sublime.packages_path()}
    def save():
        with open(r"C:\VxKexProbe\packagecontrol-result.json", "w", encoding="utf-8") as output:
            json.dump(result, output, indent=2)
    try:
        # Refuse to install into a user's normal profile.
        require(os.path.normcase(sublime.packages_path()) ==
            os.path.normcase(r"C:\VxKexProbe\Sublime4213\Data\Packages"))
        for family, kind, protocol in [(2, 1, 0), (2, 1, 6), (23, 1, 6), (2, 2, 17)]:
            with socket.socket(family, kind, protocol) as sock:
                require(not sock.get_inheritable())
                result["sockets"].append({"args": [family, kind, protocol], "non_inheritable": True})

        ws = ctypes.WinDLL("KxNet")
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetHandleInformation.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
        ws.closesocket.argtypes = [ctypes.c_size_t]
        result["ansi_unicode"] = []
        for name in ("WSASocketA", "WSASocketW"):
            create = getattr(ws, name)
            create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]
            create.restype = ctypes.c_size_t
            for flags in (1, 0x81):
                handle = create(2, 1, 6, None, 0, flags)
                require(handle != ctypes.c_size_t(-1).value)
                try:
                    actual = ctypes.c_ulong()
                    require(kernel.GetHandleInformation(handle, ctypes.byref(actual)))
                    if flags & 0x80: require(not (actual.value & 1))
                    result["ansi_unicode"].append({"api": name, "flags": flags, "handle_flags": actual.value})
                finally:
                    ws.closesocket(handle)
            # A real protocol/type error must still be reported.
            require(create(2, 1, 17, None, 0, 0x81) == ctypes.c_size_t(-1).value)

        import certifi
        from Default.install_package_control import InstallPackageControlCommand
        context = ssl.create_default_context(cafile=certifi.where())
        url = "https://download.sublimetext.com/Package%20Control.sublime-package"
        # Require verified HTTPS for both requests; no HTTP or certificate bypass.
        with urllib.request.urlopen(url, context=context, timeout=30) as response:
            package = response.read()
            result["https_status"] = response.status
        with urllib.request.urlopen(url + ".sig", context=context, timeout=30) as response:
            signature = response.read()
        command = InstallPackageControlCommand()
        require(command._verify(package, signature) == package)
        result["signature_verified"] = True
        result["sha256"] = hashlib.sha256(package).hexdigest()
        result["bytes"] = len(package)
        dest = os.path.join(sublime.installed_packages_path(), "Package Control.sublime-package")
        with open(dest + ".tmp", "wb") as output:
            output.write(package)
        os.replace(dest + ".tmp", dest)
        result["installed"] = dest
        result["ok"] = True
        save()

        def check_loaded():
            result["loaded_modules"] = [name for name in sys.modules if name.startswith("Package Control")][:25]
            result["commands"] = [cls.__name__ for cls in sublime_plugin.application_command_classes
                                  if "PackageControl" in cls.__name__]
            save()
        sublime.set_timeout_async(check_loaded, 15000)
    except BaseException:
        result["error"] = traceback.format_exc()
        save()

