"""Physical-joint ALS static mapping; source vectors, no point-fit objective."""
import argparse,csv,json,numpy as np
from scipy.spatial.transform import Rotation as R
from study_surface_mount import SurfaceMount,BASE,REF,load_pose,transform,unit,between,descendant,escape_along
from study_chest_panel_weights import chest_panels
from build_whole_carry_candidate import frame,local,compose

def mapped(s,src,sb):
    b=s.bind;p={}
    for n in s.names:
        parent=s.parents[n];p[n]=compose(local(b[n],b[parent]),p[parent]) if parent else b[n]
        if n=='Pelvis':
            delta=src['pelvis'][0]-sb['pelvis'][0];p[n]=(b[n][0]+delta,src['pelvis'][1]*sb['pelvis'][1].inv()*b[n][1],b[n][2])
        if n in ['Torso','Chest','Neck','Head']:
            donor={'Torso':'spine_01','Chest':'spine_02','Neck':'neck_01','Head':'head'}[n]
            across=(src[donor][1]*sb[donor][1].inv()).apply(np.array([1.,0,0]))
            if n=='Head':q=src[donor][1]*sb[donor][1].inv()*b[n][1]
            else:
                end={'Torso':'spine_02','Chest':'neck_01','Neck':'head'}[n];target={'Torso':'Chest','Chest':'Neck','Neck':'Head'}[n]
                if n=='Chest':across=src['upperarm_l'][0]-src['upperarm_r'][0]
                q=frame(src[end][0]-src[donor][0],across)*frame(b[target][0]-b[n][0],np.array([1.,0,0])).inv()*b[n][1]
            p[n]=(p[n][0],q,b[n][2])
        for side in ['L','R']:
            sn=side.lower();sh,u,e,w=['Shoulder_'+side,'UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side]
            for start,end,ds,de in [(sh,u,'clavicle_','upperarm_'),(u,e,'upperarm_','lowerarm_'),(e,w,'lowerarm_','hand_')]:
                if n==start:
                    dq=src[ds+sn][1]*sb[ds+sn][1].inv()
                    q=between(dq.apply(b[end][0]-b[start][0]),src[de+sn][0]-src[ds+sn][0])*dq*b[n][1]
                    p[n]=(p[n][0],q,b[n][2])
            if n=='DJ_forearm_'+side:p[n]=compose(local(b[n],b[e]),p[e])
            if n==w:
                dq=p[e][1]*b[e][1].inv();p[n]=(p[e][0]+dq.apply(b[w][0]-b[e][0]),src['hand_'+sn][1]*sb['hand_'+sn][1].inv()*b[w][1],s.hold[w][2])
            elif descendant(n,w,s.parents):p[n]=compose(local(s.hold[n],s.hold[parent]),p[parent])
    return p

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--ray',action='store_true');args=parser.parse_args()
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);s.body,wr,_=chest_panels(s);oldq=s.q;oldcenter=s.base['M4_body'][0]
    src=load_pose(s.run/'source-poses.csv',90)
    with (REF.parent/'WholeCarrySource/bind.csv').open() as f:sb={r['bone']:transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}) for r in csv.DictReader(f)}
    base=mapped(s,src,sb)
    q=frame(np.array([0.,1,0]),np.array([0.,0,1]))*frame(s.hold['M4_frontsight'][0]-s.hold['M4_rearsight'][0],s.hold['M4_sightup'][0]-s.hold['M4_rearsight'][0]).inv()
    center=base['DJ_wrist_R'][0]+q.apply(s.hold['M4_body'][0]-s.hold['DJ_wrist_R'][0])
    for n in s.held:
        v=s.hold[n];base[n]=(center+q.apply(v[0]-s.pivot),q*v[1],v[2])
    s.base=base;s.q=q;out=BASE/'AnatomicalAimStudy';delta=np.zeros(3)
    if args.ray:
        optic=center+q.apply(oldq.inv().apply(s.sight-oldcenter));axis=unit(base['M4_frontsight'][0]-base['M4_rearsight'][0]);delta=base['Camera'][0]-optic;delta-=axis*np.dot(delta,axis)
        faces=np.unique(np.r_[s.head,s.chest]);clear=escape_along(s.body.deform(base)[s.body.tris[faces]],s.gun.deform(base)[s.gun.tris]+delta,axis);delta+=axis*clear
        print('full anatomical eye registration and surface escape',delta,clear,flush=True)
    p,m=s.mount(delta);m.update(s.check(p));m.update(source='Actual ALS Rifle Aiming frame90',mapping='Chest physical segment spine_02->neck_01; all source arm vectors; fixed native segment lengths',camera=p['Camera'][0].tolist(),delta_cm=delta.tolist(),runtime_enabled=False)
    s.save(out,'ray-aligned' if args.ray else 'source-chain',p,m)
if __name__=='__main__':main()
