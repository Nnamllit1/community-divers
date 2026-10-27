"""Windows setup process helpers shared by installer acceptance checks."""
import json
import os
from pathlib import Path
import subprocess
import winreg


class SetupFixture:
    def __init__(self, work, registry_key="CommunityDivers-QA", display_name="Community Divers QA"):
        self.work = Path(work).resolve()
        self.app = self.work / ("installed-" + chr(0xe4) + " space")
        self.key = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + registry_key
        self.shortcut = Path(os.environ["APPDATA"]) / "Microsoft/Windows/Start Menu/Programs" / (display_name + ".lnk")
        self.display_name = display_name
        assert self.registration() is None, "Existing registration will not be replaced"
        assert not self.shortcut.exists(), "Existing shortcut will not be replaced"
        self.work.mkdir(parents=True, exist_ok=False)

    def registration(self):
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.key, 0,
                                winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
                names = ["InstallLocation", "DisplayName", "DisplayVersion", "UninstallString"]
                return {name: winreg.QueryValueEx(key, name)[0] for name in names}
        except FileNotFoundError:
            return None

    def command(self, executable, uninstall=False):
        executable = Path(executable).resolve()
        # NSIS consumes the final /D= or _?= tail unquoted, including spaces.
        # Pass the raw command directly to CreateProcess; never involve a shell.
        tail = " /S _?=" if uninstall else " /S /D="
        return '"' + str(executable) + '"' + tail + str(self.app)

    def setup(self, executable):
        result = subprocess.run(self.command(executable), executable=str(Path(executable).resolve()),
                                capture_output=True, timeout=60)
        assert result.returncode == 0, "Setup failed: " + str(result.returncode)

    def marker(self):
        return json.loads((self.app / ".community-divers-installed.json").read_text(encoding="utf-8"))

    def assert_installed(self):
        record = self.registration()
        assert record and Path(record["InstallLocation"]) == self.app
        assert record["DisplayName"] == self.display_name
        assert record["UninstallString"] == '"' + str(self.app / "Uninstall.exe") + '"'
        assert self.shortcut.exists() and self.marker()["state"] == "installed"

    def launch(self, name, expected=0):
        data = self.work / "launcher-data"
        capture = self.work / (name + ".png")
        result = subprocess.run([str(self.app / "CommunityDivers.exe"), "--data-dir", str(data),
                                 "--capture", str(capture)], capture_output=True, timeout=40)
        assert result.returncode == expected, "Unexpected launcher result: " + str(result.returncode)
        if expected == 0:
            assert capture.exists()
        else:
            assert not capture.exists()
            assert "setup is incomplete" in (data / "launcher-error.log").read_text(encoding="utf-8")

    def write_result(self, result):
        (self.work / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
