"""Run a named native review without overwriting previous evidence."""
from pathlib import Path
import argparse,json,subprocess
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--baseline',action='store_true');p.add_argument('--actions',action='store_true');p.add_argument('--continuous',action='store_true');a=p.parse_args()
if not a.name.replace('-','').replace('_','').isalnum():p.error('name must be alphanumeric')
r=Path(__file__).resolve().parents[1];flags=json.loads((r/'baseline.json').read_text())['flags']
args=['/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(r/'unreal/Fireline/Fireline.uproject'),'/Game/Fireline/Maps/FirelineRange','-game',*['-'+f for f in flags],'-windowed','-ResX=1280','-ResY=800','-nosound','-abslog='+str(r/'unreal/Fireline/Saved'/('review-'+a.name+'.log'))]
if a.baseline and a.continuous:p.error('choose baseline or continuous')
if a.baseline:args=[x for x in args if x!='-FirelineContinuousContactStudy']
if a.continuous and '-FirelineContinuousContactStudy' not in args:args+=['-FirelineContinuousContactStudy']
if a.actions:args+=['-FirelineBodyPoseDiagnosisAudit','-FirelineBodyPoseMotionCapture']
else:args+=['-FirelineDirectionalGameplayAudit','-FirelineFullBodyTrace','-FirelineArmedMovementAudit','-FirelineContactFixVisualAudit','-FirelineReviewName='+a.name]
# The existing action diagnostic has a fixed output path. Preserve each run.
saved=r/'unreal/Fireline/Saved';out=saved/a.name
if out.exists():p.error('evidence directory already exists; choose a new name')
if a.actions and (saved/'BodyPoseDiagnosis').exists():p.error('archive existing BodyPoseDiagnosis before running')
status=subprocess.call(args,stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT)
if a.actions and (saved/'BodyPoseDiagnosis').exists():(saved/'BodyPoseDiagnosis').rename(out)
expected=out/('samples.csv' if a.actions else 'poses.csv')
if status==0 and not expected.is_file():raise SystemExit('Review ended before evidence completed: '+str(expected))
raise SystemExit(status)
