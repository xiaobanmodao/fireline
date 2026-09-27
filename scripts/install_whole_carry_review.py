"""Install a dedicated native whole-body review entry. Leaves the game launcher intact."""
from pathlib import Path
import plistlib, subprocess
ROOT=Path(__file__).resolve().parents[1]
project=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject'
editor=Path('/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor')
for name in ['source.bin','retarget.bin','contact.bin','movement.bin','leg-scale.txt']:
    assert (project/'Saved/WholeCarryReview'/name).is_file(), f'Missing review data: {name}'
assert (project/'Binaries/Mac/libUnrealEditor-MotionReference.dylib').is_file()
app=Path.home()/'UnrealBuilds/Fireline/Launchers/FirelineWholeCarryReview.app'
contents=app/'Contents'
for folder in ['MacOS','Resources']:(contents/folder).mkdir(parents=True,exist_ok=True)
info={'CFBundleExecutable':'FirelineStudyLauncher','CFBundleIdentifier':'local.fireline.whole-carry-review',
      'CFBundleName':'测试版·M4全身动作对照','CFBundlePackageType':'APPL','CFBundleVersion':'1',
      'LSUIElement':True,'NSHighResolutionCapable':True}
config={'Candidate':'M4 idle-walk-stop / native Ryan-DJ / original ALS graph comparison; review only',
        'Editor':str(editor),'Arguments':[str(project/'ReferenceProject.uproject'),'/ALS/ALSExtras/Levels/L_Als_Playground',
        '-game','-WholeCarryReview','-windowed','-ResX=1280','-ResY=800','-nosound',
        '-ExecCmds=t.MaxFPS 45,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0']}
for name,data in [('Info.plist',info),('Resources/Study.plist',config)]:
    with (contents/name).open('wb') as f:plistlib.dump(data,f)
src=(ROOT/'scripts/LatestStudyLauncher.m').read_text()
src=src.replace('Bringing World /Game/Fireline/Maps/FirelineRange.FirelineRange up for play','WHOLE_CARRY_REVIEW_READY')
src=src.replace('[log containsString:@"Failed to enter /Game/"]','([log containsString:@"Failed to enter /ALS/"] || [log containsString:@"WHOLE_CARRY_REVIEW_INIT_FAILED"])')
output=project/'Saved/WholeCarryReview/ReviewLauncher.m';output.write_text(src)
subprocess.run(['xcrun','clang','-fobjc-arc','-framework','Cocoa',str(output),'-o',str(contents/'MacOS/FirelineStudyLauncher')],check=True)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
link=Path.home()/'Desktop/火力对决/测试版·M4全身动作对照.app'
link.parent.mkdir(parents=True,exist_ok=True)
if not link.exists():link.symlink_to(app,target_is_directory=True)
assert link.resolve()==app.resolve()
print(link)
