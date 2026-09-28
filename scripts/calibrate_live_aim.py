"""Register complete native M4 contact assembly to measured ALS Ready/Aiming.

Use evaluated source state weights, not input edges. Measure the source hand
attachment rotation once; fit a common chest translation and two elbow planes
against actual skinned surfaces. Source keys, bind, fingers and contacts stay fixed.
"""
from pathlib import Path
import csv,json,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
from scipy.optimize import least_squares
from body_proportion_geometry import Surface,signed
from build_whole_carry_candidate import unit,between,transform,descendant,mixq,frame
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';REF=BASE/'ReferenceProject/Saved/BodyProportionStudy';D=BASE/'LiveCarryProject/Saved/LiveAim/flat-source-60'
T=json.loads((REF/'target.json').read_text());B={n:transform(t) for n,t in T['reference'].items()};H={n:transform(t) for n,t in T['hold'].items()};names=T['names'];parents=T['parents'];mid=(H['DJ_wrist_L'][0]+H['DJ_wrist_R'][0])*.5;span=H['DJ_wrist_L'][0]-H['DJ_wrist_R'][0]
def read(name,step=1):
 poses={}
 for r in csv.DictReader((D/name).open(encoding='utf-8-sig')):
  i=int(r['frame'])
  if i not in chosen:continue
  poses.setdefault(i,{})[r['bone']]=(np.array([float(r[k]) for k in ['x','y','z']]),R.from_quat([float(r[k]) for k in ['qx','qy','qz','qw']]),np.array([float(r[k]) for k in ['sx','sy','sz']]))
 return poses
states=[{k:float(v) for k,v in r.items()} for r in csv.DictReader((D/'states.csv').open(encoding='utf-8-sig'))]
failed=json.loads((D.parent/'anchored-60/triangle-intersections.json').read_text());chosen=set(range(120,len(states),120));worst={}
for hit in failed:
 key=(int(states[hit['frame']]['case']),hit['part']);old=worst.get(key)
 if old is None or hit['area_cm2']>old['area_cm2']:worst[key]=hit
chosen|={v['frame'] for v in worst.values()}
fit_inputs=json.loads((ROOT/'unreal/Fireline/Docs/Validation/live-aim/calibration-inputs.json').read_text())
chosen.update(fit_inputs['critical_frames'])
raw=read('raw-poses.csv');baseline=read('poses.csv');source=read('source-poses.csv')
body=Surface(REF/'candidate');mask=np.array([n.startswith(('DJ_','LowerArm_')) or (n.startswith('UpperArm_') and np.linalg.norm(v-B[n][0])>9) for n,v in zip(body.dom,body.v)]);ids=np.flatnonzero(mask);arms=body.subset(ids);lookup={v:i for i,v in enumerate(ids)};arms.tris=np.array([[lookup[i] for i in tr] for tr in body.tris[mask[body.tris].all(axis=1)]]);gun=Surface(REF/'gun')
witnesses=json.loads((ROOT/'unreal/Fireline/Docs/Validation/live-aim/clearance-witnesses.json').read_text())
def surface_points(surface,pose):
 v=surface.deform(pose);tri=v[surface.tris];part=0 if surface is arms else 1
 extra=[np.array(w['barycentric'])@tri[w['face']] for w in witnesses if w['part']==part]
 return np.concatenate([v,tri.mean(axis=1),(tri[:,0]+tri[:,1])*.5,(tri[:,1]+tri[:,2])*.5,(tri[:,0]+tri[:,2])*.5,np.array(extra).reshape(-1,3)])
cache=[]
def assembly(p):
 q=p['DJ_wrist_R'][1]*H['DJ_wrist_R'][1].inv();q=between(q.apply(span),p['DJ_wrist_L'][0]-p['DJ_wrist_R'][0])*q
 return q,(p['DJ_wrist_L'][0]+p['DJ_wrist_R'][0])*.5
for i,p in raw.items():
 st=states[i];w=1-st['relaxed_weight']
 if w<1e-5:continue
 q,c=assembly(p);q=source[i]['hand_r'][1];h=ConvexHull(body.deform(p)[np.isin(body.dom,['Chest','Torso'])]).equations
 v=R.from_euler('z',st['view_yaw']-st['actor_yaw'],degrees=True).apply([0,np.cos(np.radians(st['aim_pitch'])),np.sin(np.radians(st['aim_pitch']))])
 cache.append((i,p,baseline[i],q,c,w,h,v,st))
# Aim reference: the native sight line must face local view and stay upright.
a=next(row for row in cache if row[-1]['case']==0 and row[-1]['aiming_weight']>.999)
forward=H['M4_frontsight'][0]-H['M4_rearsight'][0];up=H['M4_sightup'][0]-H['M4_rearsight'][0]
qdesired=frame([0,1,0],[0,0,1])*frame(forward,up).inv();k=a[3].inv()*qdesired
initial=np.r_[k.as_rotvec(),[0,0,0],[0,0]]
# Both Ready and Aiming share this registration; only actual Aiming weight
# adds the sight-axis constraint. No independent state offsets are fitted.
lengths={s:[np.linalg.norm(B[b+'_'+s][0]-B[a+'_'+s][0]) for a,b in [('UpperArm','LowerArm'),('LowerArm','DJ_wrist')]] for s in ['L','R']}
def solve(x,row):
 i,p,base,rawq,rawc,w,h,v,st=row;out=dict(p);ch=p['Chest'][1]*B['Chest'][1].inv();qa=rawq*R.from_rotvec(x[:3]);q0=base['M4_body'][1]*H['M4_body'][1].inv();c0=base['M4_body'][0]-q0.apply(H['M4_body'][0]-mid);q=mixq(q0,qa,w);q=mixq(q,frame(v,[0,0,1])*frame(forward,up).inv(),st['aiming_weight']);registration=x[3:6];ca=p['DJ_wrist_R'][0]+q.apply(mid-H['DJ_wrist_R'][0])+ch.apply(registration);c=c0*(1-w)+ca*w
 transfer=lambda n:(c+q.apply(H[n][0]-mid),q*H[n][1],H[n][2]);errs=[];metrics=[]
 for j,s in enumerate(['L','R']):
  u,e,hnd=['UpperArm_'+s,'LowerArm_'+s,'DJ_wrist_'+s];sh=p[u][0];el=p[e][0];hand=transfer(hnd);t=hand[0];axis=unit(t-sh);d=np.linalg.norm(t-sh);aa,bb=lengths[s];along=(aa*aa-bb*bb+d*d)/(2*d);rad=np.sqrt(max(1e-8,aa*aa-along*along));guide=base[e][0]*(1-w)+el*w;pole=unit(guide-sh-axis*np.dot(guide-sh,axis));ne=sh+axis*along+R.from_rotvec(axis*x[6+j]*w).apply(pole)*rad
  out[u]=(sh,between(el-sh,ne-sh)*p[u][1],p[u][2]);hr=hand[1]*B[hnd][1].inv();natural=hr.apply(unit(B[hnd][0]-B[e][0]));dq=between(natural,t-ne)*hr
  for n in [e,'DJ_forearm_'+s]:out[n]=(ne+dq.apply(B[n][0]-B[e][0]),dq*B[n][1],B[n][2])
  for n in names:
   if descendant(n,hnd,parents):out[n]=transfer(n)
  bend=np.degrees(np.arccos(np.clip(np.dot(natural,unit(t-ne)),-1,1)));errs.extend([max(0,bend-40)*3,max(0,2-(aa+bb-d))*20]);metrics.append([bend,aa+bb-d])
 for n in names:
  if n.startswith('M4_'):out[n]=transfer(n)
 # Full aim uses the source's view. Ready keeps the source's authored down angle.
 if st['aiming_weight']>.999:
  errs.extend((q.apply(unit(forward))-v)*140);errs.append(float(np.dot(q.apply(unit(up)),np.cross(v,[0,0,1])))*80)
 return out,errs,metrics
calls=0
def obj(x):
 global calls
 calls+=1;rows=[]
 for row in cache:
  n,err,_=solve(x,row);rows.extend(err)
  for sf in [arms,gun]:
   distance=np.maximum(0,1.0-signed(surface_points(sf,n),row[6]));rows.extend(np.sort(np.partition(distance,-12)[-12:])*6)
 rows.extend((x[:3]-initial[:3])*.1);rows.extend(x[3:6]*.9);rows.extend(x[6:]*.3)
 return rows
if __name__=='__main__':
 print('frames',len(cache),'initial',initial.tolist(),flush=True)
 initial_fit=np.array(fit_inputs['seed_fit'])
 fit=least_squares(lambda v:obj(np.r_[initial[:3],v]),initial_fit[3:],bounds=(np.r_[[-12,-8,-6],[-1.2]*2],np.r_[[6,22,10],[1.2]*2]),max_nfev=18,diff_step=1e-4,verbose=2)
 fit.x=np.r_[initial[:3],fit.x];aim_registration=fit.x.tolist()
 metrics=[];penetration=[];angles=[]
 for row in cache:
  n,_,m=solve(fit.x,row);metrics.append(m);penetration.append(max(0,-min(signed(surface_points(sf,n),row[6]).min() for sf in [arms,gun])))
  if row[-1]['aiming_weight']>.999:angles.append(np.degrees(np.arccos(np.clip(np.dot(unit(n['M4_frontsight'][0]-n['M4_rearsight'][0]),row[7]),-1,1))))
 m=np.array(metrics);result={'fit':fit.x.tolist(),'aim_registration':aim_registration,'rotation_driver':'original source hand_r component-space attachment; registered once at settled Aiming' ,'converged':bool(fit.success),'evaluations':fit.nfev,'frames':len(cache),'wrist_max_deg':m[:,:,0].max(axis=0).tolist(),'reach_min_cm':m[:,:,1].min(axis=0).tolist(),'torso_penetration_cm':max(penetration),'aim_axis_error_deg':[min(angles),max(angles)],'target_sha256':hashlib.sha256((REF/'target.json').read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((D/'raw-poses.csv').read_bytes()).hexdigest(),'center_constraint':'Common Ready/Aiming chest-space translation bounds [-12,-8,-6] to [6,22,10] cm; original right-hand anchor; shared weapon/hands','aim_constraint':'common weapon/hand assembly aligns sight-line/up to view at original Aiming weight; fixed length full arms solved together','scope':'offline calibration; native continuous transitions and full mesh audit required'}
 (D.parent/'surface-fit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
