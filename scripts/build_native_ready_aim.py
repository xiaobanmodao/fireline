"""Transfer Ryan RifleIK control and complete FK, then bound contact correction."""
import json,csv,argparse,numpy as np
from scipy.spatial.transform import Rotation as R
from study_surface_mount import SurfaceMount,BASE,REF,transform,unit,load_pose
from study_native_ryan_carry import native_pose
from study_native_contact_arc import arm_pairs
from study_chest_panel_weights import chest_panels
from build_whole_carry_candidate import local,compose,mixq,descendant

def source_at(source,w):
    h={n:transform(t) for n,t in source['actions']['HoldRifle-loop'][0]['bones'].items()};a={n:transform(t) for n,t in source['actions']['AimRifle'][0]['bones'].items()};p={}
    for n in source['bind']:
        parent=source['parents'][n];hh=local(h[n],h[parent]) if parent else h[n];aa=local(a[n],a[parent]) if parent else a[n]
        v=(hh[0]*(1-w)+aa[0]*w,mixq(hh[1],aa[1],w),hh[2]*(1-w)+aa[2]*w);p[n]=compose(v,p[parent]) if parent else v
    pack=dict(source);pack['actions']={'blend':[{'bones':{n:{'p':v[0].tolist(),'q':v[1].as_quat().tolist(),'s':v[2].tolist()} for n,v in p.items()}}]};return pack,p

def solve(s,source,w,aim,gaze):
    pack,control=source_at(source,w);p=native_pose(s,pack,'blend');hd=mixq(R.identity(),gaze,w)
    # The original low clavicle stance intersects this armor. Register the
    # complete branch to the already validated authored AimRifle clavicle
    # position, not a guessed shoulder offset or an increased elbow bound.
    for side in ['L','R']:
        sh='Shoulder_'+side;delta=aim[sh][0]-p[sh][0]
        for n in s.names:
            if descendant(n,sh,s.parents):
                v=p[n];p[n]=(v[0]+delta,v[1],v[2])
    p['Head']=(p['Head'][0],hd*s.bind['Head'][1],p['Head'][2]);
    for n in s.names:
        if n!='Head' and n.startswith('Camera'):p[n]=compose(local(s.bind[n],s.bind[s.parents[n]]),p[s.parents[n]])
    srcaim=transform(source['actions']['AimRifle'][0]['bones']['RifleIK']);dq=control['RifleIK'][1]*srcaim[1].inv()
    srcbind={n:transform(t) for n,t in source['bind'].items()};span=np.linalg.norm(s.bind['UpperArm_L'][0]-s.bind['UpperArm_R'][0])/np.linalg.norm(srcbind['UpperArm.L'][0]-srcbind['UpperArm.R'][0])
    srcchest=transform(source['actions']['AimRifle'][0]['bones']['Chest'])[1]*srcbind['Chest'][1].inv();shift=srcchest.inv().apply(control['RifleIK'][0]-srcaim[0]);shift[0]*=span
    center=aim['M4_body'][0]+srcchest.apply(shift);q=dq*(aim['M4_body'][1]*s.hold['M4_body'][1].inv())
    for n in s.held:
        v=s.hold[n];p[n]=(center+q.apply(v[0]-s.pivot),q*v[1],v[2])
    s.base=p;s.q=q;r,m=s.mount(np.zeros(3));arcs={}
    for side in ['L','R']:
        if not arm_pairs(s,r,side):arcs[side]=0.;continue
        u,e,hand=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];axis=unit(r[hand][0]-r[u][0]);c=r[u][0]+axis*np.dot(r[e][0]-r[u][0],axis);rad=r[e][0]-c;outward=(r['Chest'][1]*s.bind['Chest'][1].inv()).apply([1 if side=='L' else -1,0,0]);sign=np.sign(np.dot(np.cross(axis,rad),outward))
        trial=dict(arcs);trial[side]=30*sign;end,mm=s.mount(np.zeros(3),trial)
        if arm_pairs(s,end,side):raise ValueError(f'No bounded source-plane clearance {side}, alpha={w}')
        lo,hi=0.,30.
        for _ in range(16):
            mid=(lo+hi)/2;trial[side]=mid*sign;candidate,cm=s.mount(np.zeros(3),trial)
            if arm_pairs(s,candidate,side):lo=mid
            else:hi=mid
        arcs[side]=(hi+.2)*sign;r,m=s.mount(np.zeros(3),arcs)
    r,m=s.mount(np.zeros(3),arcs);m.update(s.check(r));m.update(alpha=w,source_plane_corrections=arcs)
    if max(m['wrist_deg'])>40.001:raise ValueError(f'Closest clear arc exceeds wrist bound, alpha={w}')
    if any(m[k] for k in ['head_gun_pairs','chest_gun_pairs','arm_torso_pairs','head_body_pairs']):raise ValueError(f'Surface gate failed, alpha={w}: {m}')
    return r,m

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--samples',type=int,default=5);args=parser.parse_args()
    source=json.loads((BASE/'NativeRyanCarryStudy/source.json').read_text());s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);s.body,wr,_=chest_panels(s)
    aim={n:transform(t) for n,t in json.loads((BASE/'NativeStockSeatStudy/AimRifle-pose.json').read_text()).items()};src=load_pose(s.run/'source-poses.csv',90)
    sb={x['bone']:R.from_quat([float(x[k]) for k in ['qx','qy','qz','qw']]) for x in csv.DictReader((REF.parent/'WholeCarrySource/bind.csv').open())};gaze=src['head'][1]*sb['head'].inv()
    out=BASE/'NativeReadyAimStudy';out.mkdir(exist_ok=True);report=[];frames=[]
    for w in np.linspace(0,1,args.samples):
        p,m=solve(s,source,float(w),aim,gaze);report.append(m);frames.append({n:{'p':v[0].tolist(),'q':v[1].as_quat().tolist(),'s':v[2].tolist()} for n,v in p.items()});print('CONTROL_READY_AIM',w,m,flush=True)
    (out/f'samples-{args.samples}.json').write_text(json.dumps({'frames':frames,'metrics':report,'samples':args.samples,'source_rifle_control':'RifleIK, not hand global rotation','clavicle_registration':'accepted native AimRifle position retained across Ready/Aim; not unmodified donor motion','runtime_enabled':False,'eye_registration_finished':False}))
if __name__=='__main__':main()
