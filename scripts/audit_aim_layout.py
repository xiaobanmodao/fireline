"""Measure semantic contact and optical registration; never change poses."""
import argparse,csv,gzip,hashlib,json
import numpy as np
from scipy.spatial.transform import Rotation as R
from study_surface_mount import BASE,REF,SurfaceMount,load_pose,geo
from body_proportion_geometry import Surface,rows
from audit_static_contact import distance

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--run',required=True);ap.add_argument('--prior',required=True);a=ap.parse_args()
    run=BASE/'LiveCarryProject/Saved/LiveAim'/a.run;prior=run.parent/a.prior
    states=rows(run/'states.csv');previous=rows(prior/'states.csv');assert len(states)==len(previous)
    current_rows=rows(run/'poses.csv');prior_rows=rows(prior/'poses.csv');assert len(current_rows)==len(prior_rows)
    for new,old in zip(current_rows,prior_rows):
        assert (new['frame'],new['bone'])==(old['frame'],old['bone'])
    position_columns=['x','y','z'];rotation_columns=['qx','qy','qz','qw'];scale_columns=['sx','sy','sz']
    def difference(columns):
        return float(np.max(np.abs(np.array([[float(r[k]) for k in columns] for r in current_rows])-np.array([[float(r[k]) for k in columns] for r in prior_rows]))))
    all_frame_parity={k:difference(cols) for k,cols in [('position_component_cm',position_columns),('quaternion_component',rotation_columns),('scale_component',scale_columns)]}
    assert max(all_frame_parity.values())<1e-6,'Native body/gun poses changed in the optical-only revision'
    indices=[int(np.argmin([float(r['aiming_weight']) for r in states])),int(np.argmax([float(r['aiming_weight']) for r in states]))]
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90)
    frozen=s.body;native=Surface(REF.parent/'ChestPanelStudy/candidate');assert np.array_equal(native.tris,frozen.tris)
    s.body=native
    held=s.gun.deform(s.hold);pad=np.flatnonzero(held[s.gun.tris][:,:,1].max(1)-held[:,1].min()<2.)
    groups={
        'legacy_chest_or_shoulder_or_arm':np.flatnonzero(np.isin(frozen.dom[frozen.tris],['Chest','Shoulder_R','UpperArm_R']).any(1)),
        'current_chest':np.flatnonzero((native.dom[native.tris]=='Chest').any(1)),
        'current_shoulder_junction':np.flatnonzero((native.dom[native.tris]=='Shoulder_R').any(1)),
        'current_shoulder_cap_only':np.flatnonzero((native.dom[native.tris]=='Shoulder_R').all(1)),
    }
    poses=[];parity=[]
    for i in indices:
        p=load_pose(run/'poses.csv',i);old=load_pose(prior/'poses.csv',i)
        parity.append({'frame':i,'position_cm':max(float(np.linalg.norm(p[n][0]-old[n][0])) for n in p),'rotation_deg':max(float(np.degrees((p[n][1]*old[n][1].inv()).magnitude())) for n in p)})
        bv=native.deform(p);gv=s.gun.deform(p);tri=gv[s.gun.tris[pad]]
        areas=np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)/2
        poses.append({'frame':i,'aim_weight':float(states[i]['aiming_weight']),'rear_band_faces':len(pad),'rear_band_area_centroid_cm':np.average(tri.mean(1),axis=0,weights=areas).tolist(),'surface_distances_cm':{k:distance(tri,bv[native.tris[v]]) for k,v in groups.items()},'face_counts':{k:len(v) for k,v in groups.items()}})
    optic=rows(run/'optic.csv');axis=max(float(r['optical_gun_axis_deg']) for r in optic)
    assert axis<.001,'Optical axis is not parallel to measured weapon sight axis'
    assert max(x['position_cm'] for x in parity)<1e-5 and max(x['rotation_deg'] for x in parity)<1e-5,'Body/contact pose changed'
    source=json.loads(gzip.decompress((BASE/'lyra-contacts/samples.json.gz').read_bytes()));ads=next(c for c in source if c['name']=='MM_Rifle_Idle_ADS')
    neck=[]
    for f in ads['samples']:
        p={n:np.array(v['p']) for n,v in f['bones'].items()}
        neck.append([float(np.linalg.norm(p[b]-p[c])) for c,b in [('spine_05','neck_01'),('neck_01','neck_02'),('neck_02','head')]])
    lb={r['bone']:r for r in rows(BASE/'ADSLayoutStudy/lyra.bind.csv')}
    def bind_q(n):return R.from_quat([float(lb[n][k]) for k in ['qx','qy','qz','qw']])
    # Use actual mesh bind rotations. Comparing raw world Euler angles from
    # different reference skeletons would conflate posture with bone axes.
    relative=[]
    for f in ads['samples']:
        b=f['bones'];chest=R.from_quat(b['spine_05']['q'])*bind_q('spine_05').inv();head=R.from_quat(b['head']['q'])*bind_q('head').inv()
        relative.append(float(np.degrees((chest.inv()*head).magnitude())))
    report={'run':a.run,'prior':a.prior,'pose_parity_endpoints':parity,'optical_axis_max_error_deg':axis,'basis':json.loads((run/'optic.calibration.json').read_text())['basis'],'contacts':poses,'author_camera_ray_error_range_cm':[min(float(r['author_camera_ray_cm']) for r in optic),max(float(r['author_camera_ray_cm']) for r in optic)],'legacy_eye_proxy_ray_error_range_cm':[min(float(r['proxy_error_cm']) for r in optic),max(float(r['proxy_error_cm']) for r in optic)],'eye_landmarks':'Neither proxy nor author camera is an anatomical eye bone. No inter-pupillary offset invented.','contact_semantics':'Legacy broad region is not the shoulder cap. Current dominance labels describe regions; original frozen collision masks remain unchanged.','lyra_ads_source':{'path':ads['path'],'sample_count':len(ads['samples']),'duration':ads['duration'],'data_sha256':hashlib.sha256((BASE/'lyra-contacts/samples.json.gz').read_bytes()).hexdigest(),'physical_neck_segments_cm_min':np.min(neck,axis=0).tolist(),'physical_neck_segments_cm_max':np.max(neck,axis=0).tolist(),'native_bind_exported':(BASE/'ADSLayoutStudy/lyra.bind.csv').exists(),'source_runtime_graph_evaluated':False,'source_rifle_eye_alignment_validated':False,'retargeted':False},'full_ads_completed':False,'default_changed':False}
    report['pose_parity_all_frames']=all_frame_parity
    report['lyra_ads_source']['head_deformation_relative_to_chest_deg_range']=[min(relative),max(relative)]
    out=BASE/'ADSLayoutStudy';out.mkdir(exist_ok=True);(out/(a.run+'-layout.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
