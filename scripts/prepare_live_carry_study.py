"""Build an isolated input-driven M4 study; no original model/animation writes."""
from pathlib import Path
import argparse,csv,gzip,hashlib,json,shutil,subprocess,plistlib
ROOT=Path(__file__).resolve().parents[1]
ENGINE=Path('/Users/Shared/Epic Games/UE_5.8')
p=argparse.ArgumentParser();p.add_argument('--build',action='store_true');p.add_argument('--install',action='store_true');p.add_argument('--directional',action='store_true');p.add_argument('--aim',action='store_true');p.add_argument('--aim-fit',type=Path);p.add_argument('--presentation',action='store_true');p.add_argument('--coyote',action='store_true');p.add_argument('--head-contour',action='store_true');a=p.parse_args()
if a.head_contour:a.coyote=True
if a.coyote:a.presentation=True
if a.presentation:a.aim=True
research=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';old=research/'ReferenceProject';project=research/'LiveCarryProject';source=research/'ALS-Refactored-b754d6f0f2bb03741d301f8fb88077ebfe561e17'
assert old.is_dir() and source.is_dir(),'Prepare the pinned source/proportion review first'
for d in ['Source/FirelineLiveStudy','Plugins','Content','Config','Saved/LiveCarry']:(project/d).mkdir(parents=True,exist_ok=True)
for link,target in [(project/'Plugins/ALS',source),(project/'Source/Fireline',ROOT/'unreal/Fireline/Source/Fireline'),(project/'Content/Fireline',ROOT/'unreal/Fireline/Content/Fireline')]:
 if not link.exists():link.symlink_to(target,target_is_directory=True)
 assert link.resolve()==target.resolve()
shutil.copytree(old/'Content/BodyProportionStudy',project/'Content/BodyProportionStudy',dirs_exist_ok=True)
if a.head_contour:
 shutil.copytree(old/'Content/HeadContourStudy',project/'Content/HeadContourStudy',dirs_exist_ok=True)
 head=json.loads((old/'Saved/HeadContourStudy/construction.json').read_text())
 (project/'Saved/LiveCarry/head-contour.json').write_text(json.dumps({'eye_proxy_bind_cm':head['eye_proxy_bind_cm'],'static_contact_accepted':False}))
module=project/'Source/FirelineLiveStudy'
for f in (ROOT/'scripts/live_carry_host').iterdir():shutil.copyfile(f,module/f.name)
(module/'FirelineLiveStudy.Build.cs').write_text('''using UnrealBuildTool;
public class FirelineLiveStudy : ModuleRules {
 public FirelineLiveStudy(ReadOnlyTargetRules Target) : base(Target) {
 PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
 PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","Fireline","ALS","GameplayTags","InputCore","UMG","EnhancedInput","Json","AnimGraphRuntime"});
 PrivateDependencyModuleNames.AddRange(new string[]{"Slate","SlateCore"});
 }
}
''')
(project/'Source/FirelineLiveStudyEditor.Target.cs').write_text('''using UnrealBuildTool;
public class FirelineLiveStudyEditorTarget : TargetRules {
 public FirelineLiveStudyEditorTarget(TargetInfo Target) : base(Target) {
 Type=TargetType.Editor;DefaultBuildSettings=BuildSettingsVersion.V7;IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8;ExtraModuleNames.AddRange(new string[]{"Fireline","FirelineLiveStudy"});
 }
}
''')
config={'FileVersion':3,'EngineAssociation':'5.8','Modules':[{'Name':n,'Type':'Runtime','LoadingPhase':'Default'} for n in ['Fireline','FirelineLiveStudy']], 'Plugins':[{'Name':n,'Enabled':True} for n in ['ALS','ProceduralMeshComponent','StateTree','GameplayStateTree']]}
uproject=project/'LiveCarryProject.uproject';uproject.write_text(json.dumps(config,indent=2))
for n in ['DefaultEngine.ini','DefaultInput.ini']:shutil.copyfile(old/'Config'/n,project/'Config'/n)
# Preserve registration and contact calibration, not a new optimization.
d=old/'Saved/BodyProportionStudy';r=old/'Saved/BodyProportionReview';s=old/'Saved/WholeCarrySource'
poses=json.loads(gzip.decompress((r/'poses.json.gz').read_bytes()));target=json.loads((d/'target.json').read_text());report=json.loads((r/'report.json').read_text())
def transform(row):return {'p':[float(row[k]) for k in ['x','y','z']],'q':[float(row[k]) for k in ['qx','qy','qz','qw']],'s':[float(row[k]) for k in ['sx','sy','sz']]}
with (s/'bind.csv').open(encoding='utf-8-sig') as f:source_bind={row['bone']:transform(row) for row in csv.DictReader(f)}
with (s/'poses.csv').open(encoding='utf-8-sig') as f:source_zero={row['bone']:transform(row) for row in csv.DictReader(f) if int(row['frame'])==60}
# Use actual skinned boots: heel vertices also have calf influence. A rigid
# foot-only approximation demonstrably misses their deformed contact surface.
with (d/'candidate.vertices.csv').open() as f:vertices={int(v['vertex']):[float(v[k]) for k in ['x','y','z']] for v in csv.DictReader(f)}
with (d/'candidate.weights.csv').open() as f:weights=list(csv.DictReader(f))
def cross(x,y):return [x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]]
def inverse_point(v,t):
 q=[-n for n in t['q'][:3]];delta=[v[i]-t['p'][i] for i in range(3)];c=cross(q,delta);cc=cross(q,c)
 return [(delta[i]+2*(t['q'][3]*c[i]+cc[i]))/t['s'][i] for i in range(3)]
soles={}
influences={}
for w in weights:influences.setdefault(int(w['vertex']),[]).append((w['bone'],int(w['weight'])))
for side in ['L','R']:
 bone='Foot_'+side;unique={}
 for i,weights_at_vertex in influences.items():
  if max(weights_at_vertex,key=lambda w:w[1])[0]!=bone:continue
  key=(tuple(vertices[i]),tuple(sorted(weights_at_vertex)))
  unique[key]=[{'bone':n,'weight':w/65535,'local':inverse_point(vertices[i],target['reference'][n])} for n,w in weights_at_vertex]
 assert len(unique)>4,(bone,len(unique))
 soles[side]=list(unique.values())
calibration={'target':target,'source_reference':source_bind,'source_zero':source_zero,'raw_zero':poses['raw'][0],'leg_scale':report['leg_scale'],'amplitude':report['amplitude_candidate'],'fit':report['fit'],'sole_points':soles}
clearance=ROOT/'unreal/Fireline/Docs/Validation/live-carry/clearance-fit.json'
clearance_fit=json.loads(clearance.read_text())
assert hashlib.sha256((d/'target.json').read_bytes()).hexdigest()==clearance_fit['target_reference_sha256'],'Derived bind changed; revalidate the live registration first'
for filename,expected in clearance_fit['target_surface_sha256'].items():
 assert hashlib.sha256((d/filename).read_bytes()).hexdigest()==expected,'Derived surface changed; revalidate clearance first: '+filename
calibration['live_right_pole_delta']=clearance_fit['right_elbow_pole_delta_radians']
jog_fit=json.loads((ROOT/'unreal/Fireline/Docs/Validation/live-directional/jog-fit.json').read_text())
assert jog_fit['target_reference_sha256']==clearance_fit['target_reference_sha256']
calibration['jog_registration']=jog_fit
(project/'Saved/LiveCarry/calibration.json').write_text(json.dumps(calibration))
for n in ['source.bin','source-bones.txt']:shutil.copyfile(r/n,project/'Saved/LiveCarry'/n)
aim_fit=a.aim_fit or ROOT/'unreal/Fireline/Docs/Validation/live-aim/aim-fit.json'
if a.aim:assert aim_fit.exists(),'Validated aim-fit.json is required for the aiming entry'
if aim_fit.exists():
 aim=json.loads(aim_fit.read_text());assert aim['target_sha256']==clearance_fit['target_reference_sha256']
 aim['presentation_registration']=json.loads((ROOT/'unreal/Fireline/Docs/Validation/move-aim-refinement/registration.json').read_text())
 (project/'Saved/LiveCarry/aim-registration.json').write_text(json.dumps(aim))
if a.build:subprocess.run([str(ENGINE/'Engine/Build/BatchFiles/Mac/Build.sh'),'FirelineLiveStudyEditor','Mac','Development',str(uproject),'-MaxParallelActions=2','-WaitMutex','-NoHotReloadFromIDE'],check=True)
if a.install:
 app=Path.home()/'UnrealBuilds/Fireline/Launchers'/('FirelineHeadContourStudy.app' if a.head_contour else 'FirelineCoyoteStudy.app' if a.coyote else 'FirelinePresentationStudy.app' if a.presentation else 'FirelineAimStudy.app' if a.aim else 'FirelineDirectionalStudy.app' if a.directional else 'FirelineLiveCarryStudy.app');contents=app/'Contents'
 for n in ['MacOS','Resources']:(contents/n).mkdir(parents=True,exist_ok=True)
 settings={'Candidate':'Input-driven M4 forward start-stop; derived Ryan/DJ; live original ALS graph; isolated study', 'Editor':str(ENGINE/'Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'),'Arguments':[str(uproject),'/ALS/ALSExtras/Levels/L_Als_Playground','-game','-windowed','-ResX=1280','-ResY=800','-nosound','-ExecCmds=t.MaxFPS 60,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0']}
 info={'CFBundleExecutable':'FirelineStudyLauncher','CFBundleIdentifier':'local.fireline.live-carry-study','CFBundleName':'测试版·M4实时起停','CFBundlePackageType':'APPL','CFBundleVersion':'1','LSUIElement':True,'NSHighResolutionCapable':True}
 if a.directional or a.aim:
  settings['Candidate']='M4 eight-direction walk/jog and native ALS turns; isolated derived Ryan/DJ study'
  settings['Arguments'].append('-LiveCarryDirectional')
  info.update(CFBundleIdentifier='local.fireline.directional-study',CFBundleName='测试版·M4多方向走跑')
 if a.aim:
  settings['Candidate']='M4 native ALS Relaxed/Ready/Aiming and directional carry; isolated Ryan/DJ study'
  settings['Arguments'].append('-LiveCarryAimStudy')
  info.update(CFBundleIdentifier='local.fireline.aim-study',CFBundleName='\u6d4b\u8bd5\u7248\u00b7M4\u51c6\u5907\u4e0e\u7784\u51c6')
 if a.presentation:
  settings['Candidate']='M4 world foot contacts, stable stock mount and whole-chain aim transitions; isolated study'
  settings['Arguments'].append('-LiveCarryPresentationStudy')
  info.update(CFBundleIdentifier='local.fireline.presentation-study',CFBundleName='测试版·M4移动瞄准修正')
 if a.coyote:
  settings['Candidate']='Existing M4 Presentation pose with selected Coyote and contact diagnostics; static eye/cheek correction unfinished'
  settings['Arguments'].append('-LiveCarryCoyoteStudy')
  info.update(CFBundleIdentifier='local.fireline.coyote-contact-study',CFBundleName='测试版·M4红点接触观察')
 if a.head_contour:
  settings['Candidate']='Restored original Ryan helmet profile; H compares rejected flattened contour; unchanged pose, unfinished stock/shoulder contact'
  settings['Arguments'].append('-LiveCarryHeadContourStudy')
  info.update(CFBundleIdentifier='local.fireline.head-contour-study',CFBundleName='测试版·头颈轮廓对照')
 for n,v in [('Info.plist',info),('Resources/Study.plist',settings)]:
  with (contents/n).open('wb') as f:plistlib.dump(v,f)
 text=(ROOT/'scripts/LatestStudyLauncher.m').read_text().replace('Bringing World /Game/Fireline/Maps/FirelineRange.FirelineRange up for play','LIVE_CARRY_READY').replace('[log containsString:@"Failed to enter /Game/"]','([log containsString:@"Failed to enter /ALS/"] || [log containsString:@"LIVE_CARRY_INIT_FAILED"])')
 launcher=project/'Saved/LiveCarry/Launcher.m';launcher.write_text(text)
 subprocess.run(['xcrun','clang','-fobjc-arc','-framework','Cocoa',str(launcher),'-o',str(contents/'MacOS/FirelineStudyLauncher')],check=True)
 subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
 link=Path.home()/'Desktop/火力对决'/('测试版·头颈轮廓对照.app' if a.head_contour else '测试版·M4红点接触观察.app' if a.coyote else '测试版·M4移动瞄准修正.app' if a.presentation else '测试版·M4准备与瞄准.app' if a.aim else '测试版·M4多方向走跑.app' if a.directional else '测试版·M4实时起停.app')
 if not link.exists():link.symlink_to(app,target_is_directory=True)
 assert link.resolve()==app.resolve();print(link)
print(uproject)
