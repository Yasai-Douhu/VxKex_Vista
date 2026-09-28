"""Place in an isolated Sublime package with .python-version containing 3.14.

Writes diagnostic results only to C:\\VxKexProbe\\plugin314-result.json.
"""
import ctypes
import json
import os
import sys
import time
import traceback
import sublime


def check_waitable_timer():
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    compat = ctypes.WinDLL("KxBase", use_last_error=True)
    kernel.CreateWaitableTimerW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_wchar_p]
    kernel.CreateWaitableTimerW.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    kernel.WaitForSingleObject.restype = ctypes.c_ulong
    kernel.CancelWaitableTimer.argtypes = [ctypes.c_void_p]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    compat.SetWaitableTimerEx.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_longlong), ctypes.c_long,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong,
    ]
    compat.SetWaitableTimerEx.restype = ctypes.c_int
    timer = kernel.CreateWaitableTimerW(None, False, None)
    if not timer:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        due = ctypes.c_longlong(-100000)  # 10 milliseconds, relative.
        assert compat.SetWaitableTimerEx(timer, ctypes.byref(due), 0, None, None, None, 10)
        assert kernel.WaitForSingleObject(timer, 3000) == 0
        assert kernel.WaitForSingleObject(timer, 0) == 258  # auto-reset consumed
        assert compat.SetWaitableTimerEx(timer, ctypes.byref(due), 20, None, None, None, 0)
        assert kernel.WaitForSingleObject(timer, 3000) == 0
        assert kernel.WaitForSingleObject(timer, 3000) == 0
        assert kernel.CancelWaitableTimer(timer)
        ctypes.set_last_error(0)
        assert not compat.SetWaitableTimerEx(None, ctypes.byref(due), 0, None, None, None, 0)
        assert ctypes.get_last_error() == 6  # ERROR_INVALID_HANDLE
    finally:
        kernel.CloseHandle(timer)


def plugin_loaded():
    result = {"python": sys.version, "executable": sys.executable, "pid": os.getpid()}
    try:
        import threading
        import ssl
        import sqlite3
        start = time.perf_counter()
        time.sleep(0.05)
        result["sleep_elapsed"] = time.perf_counter() - start
        event = threading.Event()
        timer = threading.Timer(0.05, event.set)
        timer.start()
        result["thread_timer"] = event.wait(3)
        timer.join()
        result["openssl"] = ssl.OPENSSL_VERSION
        with sqlite3.connect(":memory:") as db:
            result["sqlite"] = db.execute("select 42").fetchone()[0]
        check_waitable_timer()
        result["waitable_timer"] = "one-shot, auto-reset, periodic, cancel, invalid handle passed"
        result["sublime_build"] = sublime.version()
        result["ok"] = result["thread_timer"] and result["sqlite"] == 42
    except BaseException:
        result["error"] = traceback.format_exc()
    with open(r"C:\VxKexProbe\plugin314-result.json", "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
