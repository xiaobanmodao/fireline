"""Compare selected M4 operation samples with full-body source evidence.

Read-only diagnosis. Does not emit or claim a usable action retarget.
"""
from pathlib import Path
import csv,gzip,json,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';REF=BASE/'ReferenceProject/Saved';D=BASE/'LiveCarryProject/Saved/LiveM4Operations'
source=REF/'M4OperationSource/actions.json';data=json.loads(source.read_text());target=json.loads((REF/'BodyProportionStudy/target.json').read_text());hold=target['hold'];bind=target['reference']
vec=lambda p,n:np.array(p[n]['p'])
arm_lengths={s:[float(np.linalg.norm(vec(bind,b+'_'+s)-vec(bind,a+'_'+s))) for a,b in [('UpperArm','LowerArm'),('LowerArm','DJ_wrist')]] for s in ['L','R']}
result={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'target_sha256':hashlib.sha256((REF/'BodyProportionStudy/target.json').read_bytes()).hexdigest(),'native_arm_lengths_cm':arm_lengths,'clips':{},'acceptance':'FAILED direct transplant and constant operation registration; not a playable release'}
for name,clip in data['clips'].items():
 frames=clip['frames'];lengths={s:np.array([[np.linalg.norm(vec(p,b+'_'+s)-vec(p,a+'_'+s)) for a,b in [('UpperArm','LowerArm'),('LowerArm','DJ_wrist')]] for p in frames]) for s in ['L','R']}
 distance=np.array([np.linalg.norm(vec(p,'DJ_wrist_L')-vec(p,'M4_body')) for p in frames]);peak=int(distance.argmax());right=[]
 base=R.from_quat(hold['M4_body']['q']).inv().apply(vec(hold,'DJ_wrist_R')-vec(hold,'M4_body'))
 for p in frames:right.append(np.linalg.norm(R.from_quat(p['M4_body']['q']).inv().apply(vec(p,'DJ_wrist_R')-vec(p,'M4_body'))-base))
 result['clips'][name]={'duration_s':clip['duration'],'frames':len(frames),'bone_segment_length_error_cm_max':max(float(abs(lengths[s]-arm_lengths[s]).max()) for s in lengths),'right_hand_receiver_translation_error_cm_max':max(right),'left_wrist_to_receiver_origin_cm_max':float(distance[peak]),'max_distance_frame_at_60hz':peak,'max_distance_authored_time_s':peak/60}
for directory,label in [('initial-failed','direct_transplant'),('calibration-60','baseline_same_input')]:
 states=list(csv.DictReader((D/directory/'states.csv').open(encoding='utf-8-sig')));previous=0;counts={}
 for s in states:
  fail=int(s['failures']);counts[s['case']]=counts.get(s['case'],0)+fail-previous;previous=fail
 result[label]={'frames':len(states),'failed_poses':previous,'per_case_failures':counts,'state_sha256':hashlib.sha256((D/directory/'states.csv').read_bytes()).hexdigest(),'pose_sha256':hashlib.sha256((D/directory/'poses.csv').read_bytes()).hexdigest()}
lyra=json.loads(gzip.decompress((BASE/'lyra-contacts/samples.json.gz').read_bytes()))
result['full_body_reference']={c['name']:{'duration_s':c['duration'],'curves':c['curves'],'scope':'analyzed original full-body source, not transferred or accepted on current Ryan/DJ body'} for c in lyra if c['name']=='MM_Rifle_Reload'}
result['constant_fit_failed']=json.loads((D/'operation-fit.json').read_text())
# These two probes cannot certify an anatomical solution. In particular a
# successful least-squares termination is not successful contact/skin quality.
result['next_constraints']=['preserve current first-person sycgff clips and mechanical event times','third-person full-chain source registration and explicit release/contact windows, before runtime coupling','separate support-hand free reach from weapon/magazine contact; no global offsets or limb stretching','native final surface and action-interruption playback gate before new launcher/default integration']
out=ROOT/'unreal/Fireline/Docs/Validation/m4-operation-source';out.mkdir(parents=True,exist_ok=True);(out/'diagnosis.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
