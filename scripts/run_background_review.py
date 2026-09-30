"""Capture the existing native candidate without an OS window or physical input.

Mac / UE 5.8: Slate RenderOffScreen supplies a generic window; MetalOffscreenOnly
skips presentation while retaining actual GPU render buffers/screenshots.
No original assets, user windows, desktop settings or release flags are changed.
"""
from pathlib import Path
import argparse,datetime,json,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/LiveCarryProject/LiveCarryProject.uproject'
EDITOR=Path('/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor')
p=argparse.ArgumentParser();p.add_argument('--full',action='store_true',help='77-second movement/aim sequence instead of a four-view close-up');p.add_argument('--fps',type=int,choices=[30,60,120],default=30);p.add_argument('--label',default=None);p.add_argument('--coyote',action='store_true');p.add_argument('--guides',action='store_true');p.add_argument('--head-contour',action='store_true');p.add_argument('--original-head',action='store_true');p.add_argument('--head-swap',action='store_true');p.add_argument('--static-contact',action='store_true');a=p.parse_args()
if a.static_contact:
 if a.full or a.head_contour:p.error('Static contact is a separate static review, not a movement/head swap test')
 a.coyote=True
if a.head_contour:a.coyote=True
if a.head_swap and not a.head_contour:p.error('--head-swap requires --head-contour')
if a.head_swap and a.original_head:p.error('--head-swap and --original-head are separate tests')
if a.original_head and not a.head_contour:p.error('--original-head requires --head-contour')
label=a.label or 'background-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
if not label or any(not(c.isalnum() or c in '-_') for c in label):p.error('label must contain only letters, digits, - or _')
out=PROJECT.parent/'Saved/LiveAim'/f'{label}-{a.fps}'
if out.exists():p.error(f'Evidence already exists: {out}; use a fresh label')
assert PROJECT.exists() and EDITOR.exists(),'Prepare/build the native presentation study first'
# One UE instance at a time: avoid stealing an existing editor session or
# silently doubling its memory/GPU load while the user is working.
processes=subprocess.check_output(['ps','-axo','pid=,comm='],text=True)
if any(line.strip().endswith('/UnrealEditor') for line in processes.splitlines()):p.error('An UnrealEditor instance is running; leave it intact and retry after that session closes')
logdir=PROJECT.parent/'Saved/BackgroundReview';logdir.mkdir(exist_ok=True);log=logdir/(label+'.log')
args=[str(EDITOR),str(PROJECT),'/ALS/ALSExtras/Levels/L_Als_Playground','-game','-LiveCarryAimStudy','-LiveCarryPresentationStudy','-LiveCarryAudit',f'-StudyFPS={a.fps}',f'-StudyRun={label}','-RenderOffScreen','-MetalOffscreenOnly','-unattended','-nosplash','-nosound','-ResX=1100','-ResY=850','-ExecCmds=t.MaxFPS 30,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0',f'-abslog={log}']
if a.coyote:args+=['-LiveCarryCoyoteStudy']
if a.static_contact:args+=['-LiveStaticContactStudy']
if a.head_contour:args+=['-LiveCarryHeadContourStudy']
if a.original_head:args+=['-StudyOriginalHead']
if a.head_swap:args+=['-StudyHeadSwapAudit']
if a.guides:
 if not a.coyote:p.error('--guides requires --coyote')
 args+=['-StudyOpticDiagnostics']
if not a.full:args+=['-StudyDuration=5','-StudyReviewViews','-StudyCloseup']
start=time.monotonic()
with (logdir/(label+'.stdout')).open('w') as output:
 child=subprocess.Popen(args,stdout=output,stderr=subprocess.STDOUT)
 try:code=child.wait(timeout=600 if a.full else 120)
 except (subprocess.TimeoutExpired,KeyboardInterrupt):
  child.terminate()
  try:child.wait(timeout=15)
  except subprocess.TimeoutExpired:child.kill();child.wait()
  raise
text=log.read_text(errors='replace')
assert code==0,f'UE exited {code}: {log}'
assert 'LIVE_CARRY_BACKGROUND native_window=0 scripted_input=1' in text,'No proof of background window isolation'
assert 'LIVE_CARRY_AUDIT_COMPLETE' in text and 'LIVE_CARRY_POSE_INVALID' not in text,'Native playback failed; inspect retained log'
if a.coyote:
 assert 'LIVE_COYOTE_READY' in text and 'LIVE_COYOTE_INVALID' not in text,'Coyote mount/visibility check failed'
if a.static_contact:assert 'LIVE_STATIC_CONTACT_READY bones=131 original_helmet=1 moving_enabled=0' in text,'Static source candidate was not loaded'
if a.head_contour:
 assert 'LIVE_HEAD_CONTOUR_READY' in text and 'LIVE_HEAD_SWAP_INVALID' not in text,'Head contour failed native bind/pose cache validation'
 assert 'restored_original_profile=1' in text,'Rejected flattened helmet is still the active presentation'
if a.head_swap:assert 'LIVE_HEAD_SWAP_VERIFIED bones=131' in text,'Head comparison did not exercise swapping'
shots=sorted(out.glob('shot-*.png'));expected=38 if a.full else 4
assert len(shots)==expected,(len(shots),expected,log)
assert all(f.stat().st_size>10000 for f in shots),'Screenshot output missing/empty'
result={'entry':'Ryan authored static contact candidate; moving hold / eye registration unfinished' if a.static_contact else 'Head contour geometry comparison; original pose / unfinished contact' if a.head_contour else 'Coyote contact observation; baseline body pose / unfinished eye registration' if a.coyote else 'existing M4 movement/aim candidate; no new pose','directory':str(out),'log':str(log),'native_window':False,'physical_input_used':False,'static_contact':a.static_contact,'head_swap':a.head_swap,'head_contour':a.head_contour,'original_head':a.original_head,'renderer':'Metal','fps':a.fps,'full_sequence':a.full,'screenshots':len(shots),'wall_seconds':round(time.monotonic()-start,2),'visual_acceptance':False,'scope':'background capture check; screenshots still need visual review'}
(out/'background-review.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,ensure_ascii=False,indent=2))
