"""Stop the real QA setup after its first payload file, then repair using normal setup."""
import argparse
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
result = subprocess.run(fixture.command(uninstaller, uninstall=True), executable=str(uninstaller),
                        capture_output=True, timeout=60)
assert result.returncode == 0
assert fixture.registration() is None and not fixture.shortcut.exists()
assert not (fixture.app / "CommunityDivers.exe").exists()
assert (fixture.app / "mods/keep.txt").read_text(encoding="utf-8") == "keep mod"
fixture.write_result({"real_setup_terminated_mid_payload": True,
                      "partial_launcher_refused": True, "new_install_repaired": True,
                      "reinstall_repaired": True, "repaired_launcher_started": True,
                      "custom_mod_preserved": True, "repaired_install_uninstalled": True})
