"""Separate original-source viewer/capture; no Fireline asset or runtime changes."""
from pathlib import Path
import argparse,subprocess,time,plistlib,json
ROOT=Path(__file__).resolve().parents[1]
ENGINE=Path('/Users/Shared/Epic Games/UE_5.8/Engine')
PROJECT=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/ReferenceProject.uproject'
p=argparse.ArgumentParser();p.add_argument('--capture',action='store_true');p.add_argument('--install',action='store_true');p.add_argument('--raw',action='store_true',help='Disable source mesh post-process for A/B diagnosis');p.add_argument('--toggle-audit',action='store_true',help='Exercise the same live P toggle twice during background capture');p.add_argument('--label',default='original-source-20261001');a=p.parse_args()
if a.raw and a.install:p.error('Install the original post-process default; use P for live A/B')
if a.toggle_audit and (not a.capture or a.install or a.raw):p.error('Toggle audit requires capture only, starting with original post-process')
if not a.label or any(not(c.isalnum() or c in '-_') for c in a.label):p.error('Use a safe, unique label')
assert PROJECT.exists() and (PROJECT.parent.parent/'ADSLayoutStudy/lyra-equipment.json').exists()
args=[str(PROJECT),'/ALS/ALSExtras/Levels/L_Als_Playground','-game','-LyraADSReview','-nosound','-ExecCmds=t.MaxFPS 30,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0']
if a.raw:args.append('-LyraADSRaw')
if a.toggle_audit:args.append('-LyraADSToggleAudit')
if a.install:
    app=Path.home()/'UnrealBuilds/Fireline/Launchers/LyraADSReference.app';contents=app/'Contents'
    for n in ['MacOS','Resources']:(contents/n).mkdir(parents=True,exist_ok=True)
    info={'CFBundleExecutable':'FirelineStudyLauncher','CFBundleIdentifier':'local.fireline.lyra-ads-reference','CFBundleName':'参考·Lyra原版举枪','CFBundlePackageType':'APPL','CFBundleVersion':'1','LSUIElement':True,'NSHighResolutionCapable':True}
    settings={'Candidate':'Original Lyra Manny / rifle / ADS single-node clip and equipment mount; not final gameplay graph','Editor':str(ENGINE/'Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'),'Arguments':args+['-windowed','-ResX=1280','-ResY=800']}
    for n,obj in [('Info.plist',info),('Resources/Study.plist',settings)]:
        (contents/n).write_bytes(plistlib.dumps(obj))
    source=PROJECT.parent/'Saved/LyraADSReferenceLauncher.m'
    text=(ROOT/'scripts/LatestStudyLauncher.m').read_text().replace('/Game/Fireline/Maps/FirelineRange.FirelineRange','/ALS/ALSExtras/Levels/L_Als_Playground.L_Als_Playground')
    source.write_text(text)
    subprocess.run(['xcrun','clang','-fobjc-arc','-framework','Cocoa',str(source),'-o',str(contents/'MacOS/FirelineStudyLauncher')],check=True)
    subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
    link=Path.home()/'Desktop/火力对决/参考·Lyra原版举枪.app'
    if not link.exists():link.symlink_to(app,target_is_directory=True)
    assert link.resolve()==app.resolve();print(link)
if a.capture:
    running=subprocess.check_output(['ps','-axo','pid=,comm='],text=True)
    if any(l.strip().endswith('/UnrealEditor') for l in running.splitlines()):p.error('Existing UE session kept intact; close it before capture')
    out=PROJECT.parent/'Saved/LyraADSReview'/a.label
    if out.exists():p.error('Evidence exists; use a fresh label')
    log=Path('/tmp')/(a.label+'-lyra-ads.log');start=time.monotonic()
    with log.with_suffix('.stdout').open('w') as output:
        r=subprocess.run([str(ENGINE/'Binaries/Mac/UnrealEditor'),*args,'-LyraADSCapture','-ADSRun='+a.label,'-RenderOffScreen','-MetalOffscreenOnly','-unattended','-nosplash','-ResX=1100','-ResY=850','-abslog='+str(log)],stdout=output,stderr=subprocess.STDOUT,timeout=120)
    text=log.read_text(errors='replace');assert r.returncode==0 and 'LYRA_ADS_READY' in text and 'LYRA_ADS_COMPLETE' in text and 'LYRA_ADS_INIT_FAILED' not in text,log
    assert 'LYRA_ADS_BACKGROUND native_window=0 physical_input=0' in text
    assert len(list(out.glob('shot-*.png')))==4
    metadata={'run':a.label,'directory':str(out),'log':str(log),'wall_seconds':time.monotonic()-start,'native_window':False,'physical_input':False,'renderer':'Metal','original_mesh':True,'original_rifle':True,'original_equipment_transform':True,'native_single_node_clip':True,'initial_source_mesh_post_process':not a.raw,'toggle_audit':a.toggle_audit,'final_lyra_gameplay_graph':False,'fireline_retarget':False,'visual_acceptance':False}
    (out/'capture.json').write_text(json.dumps(metadata,indent=2));print(json.dumps(metadata,indent=2))
