"""Exercise real native setup screens and reject corrupted bundled payloads."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time
from setup_fixture import SetupFixture

parser = argparse.ArgumentParser()
parser.add_argument("--installer", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
fixture = SetupFixture(args.work)
installer = args.installer.resolve()


def screen(exe, name, uninstall=False, run=False):
    capture = fixture.work / (name + ".png")
    command = [str(exe), "--uninstall-dir" if uninstall else "--install-dir",
               str(fixture.app), "--capture", str(capture)]
    if run:
        command.append("--run")
    result = subprocess.run(command, capture_output=True, timeout=45)
    assert result.returncode == 0, "Native setup screen failed: " + str(result.returncode)
    assert capture.exists() and capture.stat().st_size > 1024


screen(installer, "welcome")
assert not fixture.app.exists(), "Viewing setup must not install anything"

# Flip one byte inside the embedded ZIP, keeping its declared digest unchanged.
payload = bytearray(installer.read_bytes())
assert payload[-32:-16] == b"CDSETUPBUNDLE001"
zip_bytes, metadata_bytes = struct.unpack("<QQ", payload[-16:])
offset = len(payload) - 32 - metadata_bytes - zip_bytes
assert offset >= 4096 and zip_bytes > 32
payload[offset + zip_bytes // 2] ^= 0x80
corrupt = fixture.work / "corrupt-setup.exe"
corrupt.write_bytes(payload)
result = subprocess.run(fixture.command(corrupt), capture_output=True, timeout=30)
assert result.returncode != 0 and not fixture.app.exists()
assert fixture.registration() is None and not fixture.shortcut.exists()

screen(installer, "installed", run=True)
fixture.assert_installed()
uninstaller = fixture.work / "uninstall-copy.exe"
shutil.copyfile(fixture.app / "Uninstall.exe", uninstaller)
screen(uninstaller, "uninstall-confirm", uninstall=True)
assert (fixture.app / "CommunityDivers.exe").exists()
screen(uninstaller, "uninstalled", uninstall=True, run=True)
assert not fixture.app.exists()
assert fixture.registration() is None and not fixture.shortcut.exists()

# Exercise the installed executable's handoff too: it must release its image lock
# before the external worker can remove it. The external-copy path above cannot
# establish that behavior.
fixture.setup(installer)
installed_uninstaller = fixture.app / "Uninstall.exe"
parent = subprocess.Popen([str(installed_uninstaller), "--silent"],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
parent.communicate(timeout=20)
assert parent.returncode == 0
deadline = time.monotonic() + 20
while fixture.registration() is not None or installed_uninstaller.exists():
    assert time.monotonic() < deadline, "Installed uninstaller handoff did not finish"
    time.sleep(0.05)
assert not fixture.app.exists() and not fixture.shortcut.exists()
fixture.write_result({"native_welcome_captured": True, "real_install_completion_captured": True,
                      "native_uninstall_confirmation_captured": True,
                      "real_uninstall_completion_captured": True,
                      "corrupted_payload_refused_before_install": True,
                      "installed_uninstaller_handoff_completed": True,
                      "interactive_keyboard_and_dpi_tested": False})
