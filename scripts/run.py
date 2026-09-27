"""Run the fixed gamepad baseline; later animation experiments are not enabled."""
from pathlib import Path
import argparse,json,subprocess,sys
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--editor',default='/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor');p.add_argument('--audit',choices=['gamepad','weapons','mouse','carry']);a=p.parse_args()
b=json.loads((r/'baseline.json').read_text())
args=[a.editor,str(r/'unreal/Fireline/Fireline.uproject'),b['map'],'-game',*['-'+f for f in b['flags'] if not (a.audit and a.audit!='carry' and f=='FirelineCharacterStart')],'-windowed','-ResX=1440','-ResY=900','-culture=zh-Hans']
if a.audit=='carry':
 args+=['-FirelineDirectionalGameplayAudit','-FirelineFullBodyTrace','-FirelineCarryLoopAudit','-FirelineReviewName=CarryLoopCandidate','-nosound']
elif a.audit:
 args+=['-'+{'gamepad':'FirelineGamepadAudit','weapons':'FirelineM4AnimationAudit','mouse':'FirelineRawMouseAudit'}[a.audit],'-nosound']
log=r/'unreal/Fireline/Saved'/('baseline-'+(a.audit or 'play')+'.log');log.parent.mkdir(parents=True,exist_ok=True)
args+=['-abslog='+str(log)]
raise SystemExit(subprocess.call(args))
