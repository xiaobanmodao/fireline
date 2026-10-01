"""One full physical-chain source probe; no runtime or model asset writes."""
import csv,json,gzip
import numpy as np
from pathlib import Path
from study_surface_mount import SurfaceMount,BASE,REF,transform,unit,between,descendant,R
from body_proportion_geometry import Surface,rows
from build_whole_carry_candidate import compose,local,frame
from audit_static_contact import distance

def read_bind(path):
    return {r['bone']:transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}) for r in rows(path)}

def main():
    source=BASE/'ADSLayoutStudy';src={n:transform(v) for n,v in json.loads(gzip.decompress((source/'lyra-ads-full.json.gz').read_bytes()))['frames'][0]['bones'].items()};sb=read_bind(source/'lyra.bind.csv')
    equipment=json.loads((source/'lyra-equipment.json').read_text())['actors'][0]
    mount=compose(compose(transform(equipment['components'][0]['world_at_identity_actor']),transform(equipment['equipment_relative'])),src[equipment['socket']])
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);s.body=Surface(REF.parent/'ChestPanelStudy/candidate');b=s.bind
    anchor={n:transform(v) for n,v in json.loads((BASE/'NativeStockSeatStudy/AimRifle-pose.json').read_text()).items()}
    across=src['upperarm_l'][0]-src['upperarm_r'][0];p={}
    for n in s.names:
        parent=s.parents[n];p[n]=compose(local(b[n],b[parent]),p[parent]) if parent else b[n]
        if n=='Pelvis':p[n]=anchor[n]  # Retain the already reviewed static feet/root.
        if n in ['Torso','Chest','Neck']:
            donor,end,target={'Torso':('spine_01','spine_03','Chest'),'Chest':('spine_03','neck_01','Neck'),'Neck':('neck_01','head','Head')}[n]
            q=frame(src[end][0]-src[donor][0],across)*frame(b[target][0]-b[n][0],[1.,0,0]).inv()*b[n][1]
            p[n]=(p[n][0],q,b[n][2])
        if n=='Head':p[n]=(p[n][0],src['head'][1]*sb['head'][1].inv()*b[n][1],b[n][2])
        for side in ['L','R']:
            suffix=side.lower();sh,u,e,w=['Shoulder_'+side,'UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side]
            for start,end,ds,de in [(sh,u,'clavicle_','upperarm_'),(u,e,'upperarm_','lowerarm_'),(e,w,'lowerarm_','hand_')]:
                if n==start:
                    dq=src[ds+suffix][1]*sb[ds+suffix][1].inv()
                    q=between(dq.apply(b[end][0]-b[start][0]),src[de+suffix][0]-src[ds+suffix][0])*dq*b[n][1]
                    p[n]=(p[n][0],q,b[n][2])
            if n=='DJ_forearm_'+side:p[n]=compose(local(b[n],b[e]),p[e])
            if n==w:
                dq=p[e][1]*b[e][1].inv();p[n]=(p[e][0]+dq.apply(b[w][0]-b[e][0]),src['hand_'+suffix][1]*sb['hand_'+suffix][1].inv()*b[n][1],s.hold[n][2])
            elif descendant(n,w,s.parents):p[n]=compose(local(s.hold[n],s.hold[parent]),p[parent])
    # Source weapon component axes, including the actual equipment rotation.
    # Calibrate the native rigid M4/DJ assembly at the source-mapped right hand;
    # do not separately move either hand or use an invented sight/eye point.
    q=frame(mount[1].apply([0.,1,0]),mount[1].apply([0.,0,1]))*frame(s.hold['M4_frontsight'][0]-s.hold['M4_rearsight'][0],s.hold['M4_sightup'][0]-s.hold['M4_rearsight'][0]).inv()
    center=p['DJ_wrist_R'][0]+q.apply(s.hold['M4_body'][0]-s.hold['DJ_wrist_R'][0]);raw=dict(p)
    for n in s.held:
        t=s.hold[n];p[n]=(center+q.apply(t[0]-s.pivot),q*t[1],t[2])
    s.base=p;s.q=q;out=BASE/'LyraADSChainStudy'
    metrics={'source':'Original MM_Rifle_Idle_ADS sample0 + actual WID_Rifle equipment mount','mapping':'Physical spine/neck chain collapse and full clavicle/upper/lower arm FK; original native lengths, fixed static pelvis','original_mesh_bind_weights_unchanged':True,'runtime_enabled':False,'eye_landmark_used':False,'visual_acceptance':False,'source_raw_elbows':{side:raw['LowerArm_'+side][0].tolist() for side in ['L','R']},'raw_crossings':s.check(p)}
    try:
        fitted,contact=s.mount(np.zeros(3));metrics.update(contact);metrics.update(s.check(fitted))
        held=s.gun.deform(s.hold);pad=np.flatnonzero(held[s.gun.tris][:,:,1].max(1)-held[:,1].min()<2.)
        bv=s.body.deform(fitted);gv=s.gun.deform(fitted);shoulder=np.flatnonzero((s.body.dom[s.body.tris]=='Shoulder_R').any(1))
        metrics['actual_shoulder_junction_gap_cm']=distance(gv[s.gun.tris[pad]],bv[s.body.tris[shoulder]])
        metrics['elbow_registration_displacement_cm']={side:float(np.linalg.norm(fitted['LowerArm_'+side][0]-raw['LowerArm_'+side][0])) for side in ['L','R']}
        metrics['accepted']=False  # Surface clearance does not constitute visual acceptance.
        s.save(out,'source-contact-probe',fitted,metrics)
    except ValueError as exc:
        metrics['fit_failure']=str(exc);s.save(out,'source-contact-failed',p,metrics)
if __name__=='__main__':main()
