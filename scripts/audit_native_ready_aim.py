"""Audit actual native output, imported skin and selected optic surfaces."""
import argparse,csv,json,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
from study_surface_mount import SurfaceMount,BASE,REF,transform,unit,geo
from body_proportion_geometry import Surface,rows
from build_whole_carry_candidate import local,compose,frame

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--run',required=True);args=ap.parse_args();run=BASE/'LiveCarryProject/Saved/LiveAim'/args.run
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90)
    # Freeze the original upper-arm set too: changed chest-panel ownership must
    # not move previously arm-owned triangles out of the weapon surface audit.
    upper=np.flatnonzero(np.isin(s.body.dom[s.body.tris],['UpperArm_L','UpperArm_R']).any(1))
    s.body=Surface(REF.parent/'ChestPanelStudy/candidate')
    recorded={}
    for r in rows(run/'poses.csv'):
        recorded.setdefault(int(r['frame']),{})[r['bone']]=transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]})
    states=rows(run/'states.csv');optics=rows(run/'optic.csv');assert len(recorded)==len(states)==len(optics)
    ov=np.array([[float(r[k]) for k in ['x','y','z']] for r in rows(run/'optic.vertices.csv')]);ot=np.array([[int(r[k]) for k in ['a','b','c']] for r in rows(run/'optic.triangles.csv')])
    profile=json.loads((BASE/'CoyoteContactStudy/coyote-profile.json').read_text());sightlocal=np.array(profile['sight_center_cm'])
    gb={r['bone']:transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}) for r in rows(REF/'gun.bind.csv')}
    up=unit(gb['M4_sightup'][0]-gb['M4_rearsight'][0]);direction=gb['M4_frontsight'][0]-gb['M4_rearsight'][0];mountq=frame(unit(direction-up*np.dot(up,direction)),up)
    maximum={k:0. for k in ['physical_link_error_cm','grip_error_cm','grip_error_deg','gun_axis_error_deg','wrist_deg','head_gun_pairs','chest_gun_pairs','arm_torso_pairs','head_body_pairs','upperarm_gun_pairs','head_optic_pairs','chest_optic_pairs']};unique={};failures=[];wrists=[];elbows=[];alphas=[]
    for i,p in recorded.items():
        key=hashlib.sha256(np.array([np.r_[p[n][0],p[n][1].as_quat(),p[n][2]] for n in s.names]).tobytes()).hexdigest()
        if key in unique:m=unique[key]
        else:
            m=s.check(p);m.pop('arm_torso_faces');bv=s.body.deform(p);gv=s.gun.deform(p)
            m['upperarm_gun_pairs']=len(geo.triangle_crossings(bv[s.body.tris[upper]],gv[s.gun.tris]))
            orow=optics[i];q=p['M4_body'][1]*gb['M4_body'][1].inv()*mountq;sight=np.array([float(orow['sight_'+k]) for k in 'xyz']);oc=q.apply(ov-sightlocal)+sight
            assert np.linalg.norm(q.apply([1.,0,0])-np.array([float(orow['axis_'+k]) for k in 'xyz']))<1e-5,'Optic orientation export mismatch'
            m['head_optic_pairs']=len(geo.triangle_crossings(bv[s.body.tris[s.head]],oc[ot]));m['chest_optic_pairs']=len(geo.triangle_crossings(bv[s.body.tris[s.chest]],oc[ot]))
            m['physical_link_error_cm']=0.;m['wrist_deg']=0.
            for side in ['L','R']:
                u,e,h=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side]
                for a,b in [(u,e),(e,h)]:m['physical_link_error_cm']=max(m['physical_link_error_cm'],abs(np.linalg.norm(p[a][0]-p[b][0])-np.linalg.norm(s.bind[a][0]-s.bind[b][0])))
                hand=p[h][1]*s.bind[h][1].inv();natural=hand.apply(unit(s.bind[h][0]-s.bind[e][0]));m['wrist_deg']=max(m['wrist_deg'],float(np.degrees(np.arccos(np.clip(np.dot(natural,unit(p[h][0]-p[e][0])),-1,1)))))
            m['grip_error_cm']=0.;m['grip_error_deg']=0.
            for n in s.held:
                expected=compose(local(s.hold[n],s.hold['M4_body']),p['M4_body']);m['grip_error_cm']=max(m['grip_error_cm'],float(np.linalg.norm(expected[0]-p[n][0])));m['grip_error_deg']=max(m['grip_error_deg'],float(np.degrees((expected[1]*p[n][1].inv()).magnitude())))
            m['gun_axis_error_deg']=float(np.degrees(np.arccos(np.clip(np.dot(unit(p['M4_frontsight'][0]-p['M4_rearsight'][0]),[0,1,0]),-1,1))))
            unique[key]=m
        for k in maximum:maximum[k]=max(maximum[k],m[k])
        if any(m[k] for k in maximum if k.endswith('_pairs')) or m['wrist_deg']>40.001 or m['physical_link_error_cm']>.001 or m['grip_error_cm']>.001:failures.append(i)
        wrists.append([p['DJ_wrist_'+side][0].tolist() for side in ['L','R']]);elbows.append([p['LowerArm_'+side][0].tolist() for side in ['L','R']]);alphas.append(float(states[i]['aiming_weight']))
    dt=float(states[0]['dt']);travel=np.linalg.norm(np.diff(np.array(elbows),axis=0),axis=2);report={'run':args.run,'native_frames':len(recorded),'unique_actual_poses':len(unique),'maxima':maximum,'failed_frames':failures,'max_elbow_travel_per_frame_cm':float(travel.max()),'max_elbow_speed_cm_s':float(travel.max()/dt),'aim_weight_range':[min(alphas),max(alphas)],'native_window':False,'renderer':'Metal','eye_registration_finished':False,'movement_actions_enabled':False,'default_changed':False,'visual_acceptance':False,'eye_proxy_error_range_cm':[min(float(r['proxy_error_cm']) for r in optics),max(float(r['proxy_error_cm']) for r in optics)]}
    (run/'native-ready-aim-audit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert not failures,'Failed actual native surface / link / grip gate'
if __name__=='__main__':main()
