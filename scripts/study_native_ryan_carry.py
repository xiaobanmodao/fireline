"""Evaluate the provider's authored FK on the existing proportion derivative.

No source mesh, bind or skin edits. Saved diagnostic only, not game integration.
"""
import json
import numpy as np
from scipy.spatial.transform import Rotation as R
from study_surface_mount import SurfaceMount,BASE,transform,unit,between,descendant
from build_whole_carry_candidate import local,compose,frame

def native_pose(study,source,action):
    sb={n:transform(t) for n,t in source['bind'].items()};sp={n:transform(t) for n,t in source['actions'][action][0]['bones'].items()}
    b,h,parents=study.bind,study.hold,study.parents;p={}
    for n in study.names:
        parent=parents[n];p[n]=compose(local(b[n],b[parent]),p[parent]) if parent else b[n]
        source_name=n.replace('_L','.L').replace('_R','.R')
        if source_name in sp:
            sd=sp[source_name][1]*sb[source_name][1].inv();pos=p[n][0]
            if n=='Pelvis':pos=b[n][0]+sp[source_name][0]-sb[source_name][0]
            elif parent:
                snp=source['parents'].get(source_name)
                if snp in sp:
                    pd=sp[snp][1]*sb[snp][1].inv()
                    # The original AimRifle authors clavicle lift in translation;
                    # dropping it loses the complete native shoulder action.
                    shift=pd.inv().apply(sp[source_name][0]-sp[snp][0])-(sb[source_name][0]-sb[snp][0])
                    target_pd=p[parent][1]*b[parent][1].inv();pos=pos+target_pd.apply(shift)
            p[n]=(pos,sd*b[n][1],b[n][2])
        for side in ['L','R']:
            e,w='LowerArm_'+side,'DJ_wrist_'+side
            if n=='DJ_forearm_'+side:p[n]=compose(local(b[n],b[e]),p[e])
            if n==w:
                dq=p[e][1]*b[e][1].inv();hand=sp['UpperHand.'+side][1]*sb['UpperHand.'+side][1].inv()
                p[n]=(p[e][0]+dq.apply(b[w][0]-b[e][0]),hand*b[w][1],h[w][2])
            elif descendant(n,w,parents):p[n]=compose(local(h[n],h[parent]),p[parent])
    return p

def main():
    source=json.loads((BASE/'NativeRyanCarryStudy/source.json').read_text());s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);out=BASE/'NativeRyanCarryStudy'
    for action in ['HoldRifle-loop','AimRifle']:
        p=native_pose(s,source,action);s.base=p
        q=p['DJ_wrist_R'][1]*s.hold['DJ_wrist_R'][1].inv()
        if action=='AimRifle':
            forward=s.hold['M4_frontsight'][0]-s.hold['M4_rearsight'][0];up=s.hold['M4_sightup'][0]-s.hold['M4_rearsight'][0]
            q=frame(np.array([0.,1,0]),np.array([0.,0,1]))*frame(forward,up).inv()
        origin=p['DJ_wrist_R'][0]+q.apply(s.hold['M4_body'][0]-s.hold['DJ_wrist_R'][0])
        for n in s.held:
            v=s.hold[n];p[n]=(origin+q.apply(v[0]-s.pivot),q*v[1],v[2])
        s.q=q;s.base=p
        result,metrics=s.mount(np.zeros(3));metrics.update(s.check(result));metrics.update({'source_action':action,'gun_forward':unit(result['M4_frontsight'][0]-result['M4_rearsight'][0]).tolist(),'native_author_camera':result['Camera'][0].tolist(),'bind_weights_mesh_unchanged':True,'runtime_enabled':False,'visual_acceptance':False})
        s.save(out,action.replace('-loop',''),result,metrics)
if __name__=='__main__':main()
