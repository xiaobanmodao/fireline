"""Measured native clearance calibration; requires retained local trace data."""
from pathlib import Path
import sys,json,csv,numpy as np
from scipy.spatial import ConvexHull
from scipy.optimize import minimize,LinearConstraint
from scipy.spatial.transform import Rotation as R
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from body_proportion_geometry import Surface
root=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';ref=root/'ReferenceProject/Saved/BodyProportionStudy';base=root/'LiveCarryProject/Saved/LiveAim';d=base/'final-30';states=list(csv.DictReader((d/'states.csv').open(encoding='utf-8-sig')));selected={i for i,s in enumerate(states) if int(s['case']) in [4,8]};poses={}
for r in csv.DictReader((d/'poses.csv').open(encoding='utf-8-sig')):
 i=int(r['frame'])
 if i not in selected:continue
 poses.setdefault(i,{})[r['bone']]={'p':[float(r[k]) for k in 'xyz'],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}
body=Surface(ref/'candidate');gun=Surface(ref/'gun');b=json.loads((ref/'target.json').read_text())['reference'];need=0;worst=None;aa=[];bb=[]
for i,p in poses.items():
 h=ConvexHull(body.deform(p)[np.isin(body.dom,['Chest','Torso'])]).equations;v=gun.deform(p);dots=np.einsum("ij,kj->ik",v,h[:,:3],optimize=False)+h[:,3];rows=dots.max(axis=1)<.15
 if not rows.any():continue
 w=1-float(states[i]['relaxed_weight']);ch=R.from_quat(p['Chest']['q'])*R.from_quat(b['Chest']['q']).inv();ds=dots[rows];jj=ds.argmax(axis=1);aa.extend(ch.inv().apply(h[jj,:3])*w);bb.extend(.15-ds[np.arange(len(jj)),jj]);direction=(R.from_quat(p['Chest']['q'])*R.from_quat(b['Chest']['q']).inv()).apply([0,1,0])*w;slope=h[:,:3]@direction
 ds=dots[rows];delta=np.where(slope[None,:]>1e-8,(.15-ds)/np.maximum(slope,1e-8),np.inf).min(axis=1).max()
 if delta>need:need=float(delta);worst=i
opt=minimize(lambda x:.5*np.dot(x,x),[0,need,0],jac=lambda x:x,constraints=[LinearConstraint(np.array(aa),np.array(bb),np.inf)],bounds=[(-2,2)]*3,method='SLSQP',options={'ftol':1e-12});assert opt.success,opt.message
print({'minimum_translation_cm':opt.x.tolist(),'forward_only_cm':need,'frame':worst},flush=True)
fit=json.loads((base/'clearance-fit.json').read_text());fit['fit'][3:6]=(np.array(fit['fit'][3:6])+opt.x).tolist();fit['aim_registration']=fit['fit'].copy();fit['low_fps_clearance']={'minimum_translation_cm':opt.x.tolist(),'forward_only_required_cm':need,'margin_cm':.15,'source':'final-30','worst_frame':worst};(base/'lowfps-fit.json').write_text(json.dumps(fit,indent=2))
