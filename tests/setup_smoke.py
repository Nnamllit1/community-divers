"""Exercise the built installer/uninstaller through their silent command-line interface."""
import argparse, ctypes, hashlib, json, shutil, subprocess, winreg
from ctypes import wintypes
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--installer',required=True,type=Path)
p.add_argument('--work',required=True,type=Path)
p.add_argument('--registry-key',default='CommunityDivers')
p.add_argument('--display-name',default='Community Divers')
a=p.parse_args()
work=a.work.resolve();work.mkdir(parents=True,exist_ok=False)
app=work/('installed-'+chr(0xe4)+' space')
registry='Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\'+a.registry_key
flags=winreg.KEY_READ|winreg.KEY_WOW64_64KEY
def registration():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,registry,0,flags) as key:
            return {name:winreg.QueryValueEx(key,name)[0] for name in ['InstallLocation','DisplayName','DisplayVersion','UninstallString']}
    except FileNotFoundError:return None
assert registration() is None,'The test registration already exists; refusing to replace it'
import os
shortcut=Path(os.environ['APPDATA'])/'Microsoft/Windows/Start Menu/Programs'/(a.display_name+'.lnk')
assert not shortcut.exists(),'The test shortcut already exists'
def run(args,expected=0):
    r=subprocess.run(list(map(str,args)),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
    assert (r.returncode==0)==(expected==0),'Unexpected setup result: '+str(r.returncode)+' '+r.stderr
    return r
def setup(destination,expected=0):
    # NSIS /D= consumes the final, unquoted command-line tail. No shell is involved.
    command='"'+str(a.installer.resolve())+'" /S /D='+str(destination)
    r=subprocess.run(command,executable=str(a.installer.resolve()),capture_output=True,timeout=60)
    assert (r.returncode==0)==(expected==0),'Unexpected setup exit code: '+str(r.returncode)
    return r
def uninstall(executable,destination,expected=0):
    command='"'+str(executable)+'" /S _?='+str(destination)
    r=subprocess.run(command,executable=str(executable),capture_output=True,timeout=60)
    assert (r.returncode==0)==(expected==0),'Unexpected uninstall exit code: '+str(r.returncode)
    return r
# An unrelated folder cannot be adopted by setup.
foreign=work/'foreign';foreign.mkdir();(foreign/'keep.txt').write_text('keep',encoding='utf-8')
setup(foreign,expected=1)
assert (foreign/'keep.txt').read_text(encoding='utf-8')=='keep' and len(list(foreign.iterdir()))==1
setup(app)
record=registration()
assert record and Path(record['InstallLocation'])==app and record['DisplayName']==a.display_name
assert record['UninstallString']=='"'+str(app/'Uninstall.exe')+'"'
assert shortcut.exists()
assert json.loads((app/'.community-divers-installed.json').read_text(encoding='utf-8'))['state']=='installed'
# Use the installed native application without sending desktop input.
data=work/'user-data';data.mkdir();(data/'keep.txt').write_text('keep data',encoding='utf-8')
run([app/'CommunityDivers.exe','--data-dir',data,'--capture',work/'installed.png'])
assert (work/'installed.png').exists()
uninstaller=work/'uninstall-test.exe';shutil.copyfile(app/'Uninstall.exe',uninstaller)
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
kernel.CreateFileW.restype=wintypes.HANDLE
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
h=kernel.CreateFileW(str(app/'.community-divers-launcher.lock'),0xC0000000,0,None,3,0x80,None)
assert h!=ctypes.c_void_p(-1).value,'Could not hold the real application lock'
try:
    uninstall(uninstaller,app,expected=1)
    assert (app/'CommunityDivers.exe').exists() and registration()
finally:kernel.CloseHandle(h)
# Preserve both unknown user files and a modified launcher resource.
custom=app/'docs/release-notes.md'
custom.write_text(custom.read_text(encoding='utf-8')+'\ncustom notes\n',encoding='utf-8')
(app/'mods').mkdir();(app/'mods/keep.txt').write_text('keep mod',encoding='utf-8')
(app/'game-copy').mkdir();(app/'game-copy/keep.bin').write_bytes(b'keep game')
uninstall(uninstaller,app)
assert not (app/'CommunityDivers.exe').exists() and not (app/'CommunityDiversUpdate.exe').exists()
assert not (app/'install-inventory.json').exists() and not (app/'.community-divers-installed.json').exists()
assert not (app/'Uninstall.exe').exists() and not shortcut.exists() and registration() is None
assert custom.read_text(encoding='utf-8').endswith('custom notes\n')
assert (app/'mods/keep.txt').read_text(encoding='utf-8')=='keep mod'
assert (app/'game-copy/keep.bin').read_bytes()==b'keep game'
assert (data/'keep.txt').read_text(encoding='utf-8')=='keep data'
result={'installer':True,'unicode_path':True,'app_registration':True,'start_menu_shortcut':True,'native_installed_app_started':True,
        'active_launcher_refused':True,'unrelated_directory_refused':True,'uninstaller':True,'custom_files_preserved':True,
        'mods_preserved':True,'game_copy_preserved':True,'user_data_preserved':True,'interactive_wizard_input_tested':False}
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
