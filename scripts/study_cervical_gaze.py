"""Preserve accepted carry; solve a fixed-length cervical link and source gaze."""
import csv,json,numpy as np
from study_surface_mount import SurfaceMount,BASE,REF,load_pose,transform,unit,between,descendant,R
from study_chest_panel_weights import chest_panels
from build_whole_carry_candidate import compose,local

def main():
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);oldq=s.q;oldcenter=s.base['M4_body'][0]
    s.body,_,_=chest_panels(s);p={n:transform(v) for n,v in json.loads((BASE/'NativeStockSeatStudy/AimRifle-pose.json').read_text()).items()}
    src=load_pose(s.run/'source-poses.csv',90)
    with (REF.parent/'WholeCarrySource/bind.csv').open() as f:sb={r['bone']:transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}) for r in csv.DictReader(f)}
    sd=lambda n:src[n][1]*sb[n][1].inv()
    chest=p['Chest'][1]*s.bind['Chest'][1].inv();preferred=chest*sd('spine_02').inv()*sd('neck_01');head=sd('head')
    neck=p['Neck'][0];length=np.linalg.norm(s.bind['Head'][0]-s.bind['Neck'][0]);guide=preferred.apply(s.bind['Head'][0]-s.bind['Neck'][0])
    gq=p['M4_body'][1]*s.hold['M4_body'][1].inv();optic=p['M4_body'][0]+gq.apply(oldq.inv().apply(s.sight-oldcenter))
    # Author camera position + a 6.4cm inter-pupillary design reference; the
    # opaque stylized helmet has no eye bone. Do not call this an anatomical eye.
    eye_offset=head.apply(s.bind['Camera'][0]-s.bind['Head'][0]+np.array([-3.2,0.,0.]))
    x=optic[0]-eye_offset[0]-neck[0];y=guide[1];z2=length*length-x*x-y*y;assert z2>0
    endpoint=neck+np.array([x,y,np.sqrt(z2)]);neck_d=between(guide,endpoint-neck)*preferred
    p['Neck']=(neck,neck_d*s.bind['Neck'][1],p['Neck'][2]);p['Head']=(endpoint,head*s.bind['Head'][1],p['Head'][2])
    for n in s.names:
        if n not in ['Neck','Head'] and descendant(n,'Head',s.parents):p[n]=compose(local(s.bind[n],s.bind[s.parents[n]]),p[s.parents[n]])
    eye=endpoint+eye_offset;lift=eye[2]-optic[2]
    m=s.check(p);m.update(neck_head_length_cm=length,length_error_cm=float(abs(np.linalg.norm(endpoint-neck)-length)),neck_correction_from_source_deg=float(np.degrees((neck_d*preferred.inv()).magnitude())),head_source_deform_euler_xyz_deg=head.as_euler('xyz',degrees=True).tolist(),optic_lift_cm=float(lift),right_view_design_origin_cm=eye.tolist(),current_optic_center_cm=optic.tolist(),source='ALS Aiming head orientation and neck relative to spine_02; accepted NativeStockSeat carry',hands_gun_upperbody_unchanged=True,runtime_enabled=False,visual_acceptance=False)
    s.save(BASE/'CervicalGazeStudy','coyote',p,m)
if __name__=='__main__':main()
