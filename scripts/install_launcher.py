"""Build a separate Mac launcher for this frozen project directory."""
from pathlib import Path
import json,plistlib,subprocess
r=Path(__file__).resolve().parents[1];b=json.loads((r/'baseline.json').read_text())
app=Path.home()/'UnrealBuilds/Fireline/Launchers/FirelineGamepadBaseline.app';c=app/'Contents'
for f in ['MacOS','Resources']:(c/f).mkdir(parents=True,exist_ok=True)
info={'CFBundleExecutable':'FirelineStudyLauncher','CFBundleIdentifier':'local.fireline.gamepad-baseline','CFBundleName':'火力对决·手柄基线','CFBundlePackageType':'APPL','CFBundleVersion':'1','LSUIElement':True,'NSHighResolutionCapable':True}
config={'Candidate':b['id']+'; reconstructed, not exact historical restoration','Editor':'/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor','Arguments':[str(r/'unreal/Fireline/Fireline.uproject'),b['map'],'-game',*['-'+f for f in b['flags']],'-windowed','-ResX=1440','-ResY=900','-culture=zh-Hans']}
for path,obj in [(c/'Info.plist',info),(c/'Resources/Study.plist',config)]:
 with path.open('wb') as f:plistlib.dump(obj,f)
subprocess.run(['xcrun','clang','-fobjc-arc','-framework','Cocoa',str(r/'scripts/LatestStudyLauncher.m'),'-o',str(c/'MacOS/FirelineStudyLauncher')],check=True)
subprocess.run(['xattr','-cr',str(app)],check=True);subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
d=Path.home()/'Desktop/火力对决/开发版·手柄基线.app';d.parent.mkdir(parents=True,exist_ok=True)
if not d.exists():d.symlink_to(app,target_is_directory=True)
else:assert d.resolve()==app
print(d)
