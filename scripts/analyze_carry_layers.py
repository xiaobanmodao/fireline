"""Summarize evaluated layer traces. Numerical checks are not visual acceptance."""
import argparse,csv,json,math
from collections import defaultdict
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('--out',type=Path);a=p.parse_args()
LOWER=['Root','Pelvis','UpperLeg_L','LowerLeg_L','Foot_L','UpperLeg_R','LowerLeg_R','Foot_R']
def load(folder):
 d=defaultdict(dict)
 for r in csv.DictReader((folder/'poses.csv').open()):d[int(r['group']),float(r['time']),int(r['stage'])][r['bone']]=tuple(float(r[k]) for k in ['x','y','z'])
 return d
def summary(folder,d):
 errors=[];parity=[];lengths=defaultdict(list)
 for (g,t,s),b in d.items():
  if s==8 and (g,t,5) in d:errors.extend(math.dist(b[n],d[g,t,5][n]) for n in LOWER)
  if s==3 and (g,t,4) in d:parity.extend(math.dist(b[n],d[g,t,4][n]) for n in LOWER+['LowerArm_L','LowerArm_R','DJ_wrist_L','DJ_wrist_R'])
  if s==4:
   for side in ['L','R']:
    lengths['upper_'+side].append(math.dist(b['UpperArm_'+side],b['LowerArm_'+side]))
    lengths['forearm_'+side].append(math.dist(b['LowerArm_'+side],b['DJ_wrist_'+side]))
 c=list(csv.DictReader((folder/'contact.csv').open()))
 return {'carry_lower_body_change_max_cm':max(errors,default=None),'render_trace_position_error_max_cm':max(parity),'contact_error_max_cm':max(float(x['contact_error']) for x in c),'wrist_constraint_angle_max_deg':max(float(x['wrist_bend']) for x in c),'forearm_length_solver_error_max_cm':max(float(x['bone_error']) for x in c),'rendered_segment_length_ranges_cm':{k:[min(v),max(v)] for k,v in lengths.items()},'evaluated_frames':sum(s==4 for g,t,s in d)}
before=load(a.before);after=load(a.after)
r={'before':summary(a.before,before),'after':summary(a.after,after),'stage_names':{'0':'ground blend including authored transitions','1':'mobility','2':'upper action layers','3':'final pose after foot solve','4':'rendered skeletal component','5':'body solve','6':'existing Contact clip blend before runtime registration (NOT raw donor)','7':'carry registration onto body','8':'hand/weapon targets before final elbow solve'},'notes':['Distances are skeletal component centimetres; surface collision is not measured.','Joint lengths do not by themselves establish anatomy or art acceptance.']}
# First-person component feedback must be unaffected by body base registration.
v1={(x['group'],x['time']):x for x in csv.DictReader((a.before/'view.csv').open())};v2={(x['group'],x['time']):x for x in csv.DictReader((a.after/'view.csv').open())};keys=v1.keys()&v2.keys();columns=[k for k in next(iter(v1.values())) if k.startswith(('view_','feedback_','camera_'))]
r['matched_view_frames']=len(keys);r['first_person_component_feedback_max_delta']=max(abs(float(v1[k][c])-float(v2[k][c])) for k in keys for c in columns)
assert r['after']['carry_lower_body_change_max_cm']<.001
assert r['after']['render_trace_position_error_max_cm']<.001
assert r['after']['contact_error_max_cm']<.001
assert r['after']['forearm_length_solver_error_max_cm']<.001
assert r['first_person_component_feedback_max_delta']<.001
print(json.dumps(r,indent=2))
if a.out:a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(r,indent=2)+'\n')
