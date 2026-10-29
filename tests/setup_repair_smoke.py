"""Stop the real QA setup after its first payload file, then repair using normal setup."""
import argparse
from contextlib import contextmanager
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time
from setup_fixture import SetupFixture

parser = argparse.ArgumentParser()
parser.add_argument("--installer", type=Path, required=True)
parser.add_argument("--interrupted-installer", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
fixture = SetupFixture(args.work)
ready = fixture.app / ".community-divers-test-ready"


@contextmanager
def delete_shared_reader(path):
    """Keep the owned launcher alive without preventing its uninstall disposition."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    # FILE_READ_ATTRIBUTES; FILE_SHARE_READ | WRITE | DELETE; OPEN_EXISTING.
    handle = kernel.CreateFileW(str(path), 0x80, 7, None, 3, 0x80, None)
    if handle in (None, ctypes.c_void_p(-1).value):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        yield handle
    finally:
        if not kernel.CloseHandle(handle):
            raise ctypes.WinError(ctypes.get_last_error())


def delete_pending(handle):
    class FileStandardInfo(ctypes.Structure):
        _fields_ = [("AllocationSize", ctypes.c_longlong), ("EndOfFile", ctypes.c_longlong),
                    ("NumberOfLinks", wintypes.DWORD), ("DeletePending", ctypes.c_ubyte),
                    ("Directory", ctypes.c_ubyte)]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetFileInformationByHandleEx.argtypes = [wintypes.HANDLE, ctypes.c_int,
                                                   ctypes.c_void_p, wintypes.DWORD]
    kernel.GetFileInformationByHandleEx.restype = wintypes.BOOL
    info = FileStandardInfo()
    if not kernel.GetFileInformationByHandleEx(handle, 1, ctypes.byref(info), ctypes.sizeof(info)):
        raise ctypes.WinError(ctypes.get_last_error())
    return bool(info.DeletePending)


def file_metadata(path):
    evidence = {"path": str(path)}
    try:
        stat = path.stat()
        evidence.update({"exists": True, "file_id": stat.st_ino, "device": stat.st_dev,
                         "bytes": stat.st_size, "mtime_ns": stat.st_mtime_ns,
                         "file_attributes": stat.st_file_attributes})
        with path.open("rb") as stream:
            digest = hashlib.sha256()
            for chunk in iter(lambda: stream.read(65536), b""):
                digest.update(chunk)
            evidence["sha256"] = digest.hexdigest()
    except FileNotFoundError:
        evidence["exists"] = False
    except OSError as error:
        evidence["error"] = str(error)
    return evidence


def uninstall_repaired(uninstaller):
    launcher = fixture.app / "CommunityDivers.exe"
    marker = fixture.app / ".community-divers-installed.json"
    inventory = json.loads((fixture.app / "install-inventory.json").read_text(encoding="utf-8"))
    evidence = {"launcher_before": file_metadata(launcher), "marker_before": fixture.marker(),
                "marker_file_before": file_metadata(marker),
                "launcher_inventory_before": [item for item in inventory["files"]
                                              if item["path"] == "CommunityDivers.exe"]}
    try:
        with delete_shared_reader(launcher) as reader:
            result = subprocess.run(fixture.command(uninstaller, uninstall=True), executable=str(uninstaller),
                                    capture_output=True, timeout=60)
            evidence.update({"uninstall_returncode": result.returncode,
                             "uninstall_stdout": result.stdout.decode("utf-8", errors="replace"),
                             "uninstall_stderr": result.stderr.decode("utf-8", errors="replace")})
            assert result.returncode == 0, "Uninstall failed"
            assert fixture.registration() is None and not fixture.shortcut.exists()
            evidence["delete_pending_on_original_reader"] = delete_pending(reader)
            assert evidence["delete_pending_on_original_reader"], "Owned launcher was preserved, not marked for deletion"
            # The old immediate Path.exists assertion fails deterministically here.
            evidence["path_exists_while_reader_open"] = launcher.exists()
            assert evidence["path_exists_while_reader_open"]
        evidence["original_reader_closed"] = True
        started = time.monotonic()
        while launcher.exists():
            assert time.monotonic() - started < 20, "Launcher remained after uninstall and reader close"
            time.sleep(0.05)
        evidence["removal_wait_seconds"] = time.monotonic() - started
        evidence["launcher_removed"] = True
    except BaseException as error:
        evidence["failure"] = type(error).__name__ + ": " + str(error)
        if isinstance(error, subprocess.TimeoutExpired):
            evidence["uninstall_stdout"] = (error.stdout or b"").decode("utf-8", errors="replace")
            evidence["uninstall_stderr"] = (error.stderr or b"").decode("utf-8", errors="replace")
        raise
    finally:
        evidence.update({"launcher_after": file_metadata(launcher), "marker_file_after": file_metadata(marker)})
        # Emit diagnostics even on failure: subsequent CI artifact steps may be skipped.
        with (fixture.work / "uninstall-diagnostic.json").open("x", encoding="utf-8") as stream:
            json.dump(evidence, stream, indent=2)
        print(json.dumps(evidence, indent=2), flush=True)


def interrupt_setup():
    assert not ready.exists()
    exe = args.interrupted_installer.resolve()
    process = subprocess.Popen(fixture.command(exe), executable=str(exe),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 30
        while not ready.exists():
            assert process.poll() is None, "Interruption fixture exited before its checkpoint"
            assert time.monotonic() < deadline, "Setup did not reach its partial-payload checkpoint"
            time.sleep(0.02)
        assert process.poll() is None and fixture.marker()["state"] == "preparing"
        assert (fixture.app / "CommunityDivers.exe").exists()
        # Popen retains the process handle: this cannot target a recycled PID.
        process.kill()
        process.communicate(timeout=10)
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
    ready.unlink()


interrupt_setup()
assert fixture.registration() is None and not (fixture.app / "build-info.json").exists()
fixture.launch("incomplete-new-install", expected=1)
(fixture.app / "mods").mkdir()
(fixture.app / "mods/keep.txt").write_text("keep mod", encoding="utf-8")
fixture.setup(args.installer)
fixture.assert_installed()
fixture.launch("repaired-new-install")
assert (fixture.app / "mods/keep.txt").read_text(encoding="utf-8") == "keep mod"
# Interrupt a reinstall over a working registered copy as a separate case.
version = fixture.registration()["DisplayVersion"]
interrupt_setup()
assert fixture.registration()["DisplayVersion"] == version
fixture.launch("incomplete-reinstall", expected=1)
fixture.setup(args.installer)
fixture.assert_installed()
fixture.launch("repaired-reinstall")
assert (fixture.app / "mods/keep.txt").read_text(encoding="utf-8") == "keep mod"
uninstaller = fixture.work / "uninstall-repaired.exe"
shutil.copyfile(fixture.app / "Uninstall.exe", uninstaller)
uninstall_repaired(uninstaller)
assert (fixture.app / "mods/keep.txt").read_text(encoding="utf-8") == "keep mod"
fixture.write_result({"real_setup_terminated_mid_payload": True,
                      "partial_launcher_refused": True, "new_install_repaired": True,
                      "reinstall_repaired": True, "repaired_launcher_started": True,
                      "custom_mod_preserved": True, "repaired_install_uninstalled": True,
                      "uninstall_delete_pending_reader_verified": True})
