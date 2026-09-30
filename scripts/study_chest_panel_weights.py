"""Isolate rigid chest-panel ownership from automatic arm-envelope weights.

Face sets for collision comparison are frozen BEFORE any weight edits, so
changing a vertex's dominant owner cannot hide intersections from the audit.
"""
import json,copy
from collections import defaultdict
import numpy as np
from study_surface_mount import SurfaceMount,BASE,REF,transform,R

def chest_panels(study):
    s=study.body;author=json.loads((REF.parent/'HeadContourStudy/mesh-reference.json').read_text())['candidate'];mats={tuple(sorted(t)):author['materials'][m] for t,m in zip(author['triangles'],author['face_materials'])};ids=study.auth[s.tris]
    materials=[mats[tuple(sorted(t))] for t in ids];norm=np.cross(s.v[s.tris][:,1]-s.v[s.tris][:,0],s.v[s.tris][:,2]-s.v[s.tris][:,0]);norm/=np.linalg.norm(norm,axis=1)[:,None]
    edges=defaultdict(list)
    for i,t in enumerate(ids):
        for a,b in zip(t,np.roll(t,-1)):edges[tuple(sorted((int(a),int(b))))].append(i)
    seeds=[i for i,t in enumerate(s.tris) if materials[i]=='M_Player' and any(n in ['Chest','Torso'] for n in s.dom[t]) and any(n.startswith(('UpperArm_','Shoulder_')) for n in s.dom[t])]
    selected=set(seeds);todo=list(seeds)
    while todo:
        i=todo.pop()
        for a,b in zip(ids[i],np.roll(ids[i],-1)):
            for j in edges[tuple(sorted((int(a),int(b))))]:
                if j in selected or materials[j]!='M_Player':continue
                if abs(np.dot(norm[i],norm[j]))>1-1e-6 and abs(np.dot(s.v[s.tris[j,0]]-s.v[s.tris[i,0]],norm[i]))<.005:
                    selected.add(j);todo.append(j)
    auth_ids=set(int(x) for x in ids[list(selected)].ravel());weights=copy.deepcopy(author['weights']);changed=[]
    for i in sorted(auth_ids):
        w=weights[i];removed=sum(v for n,v in w.items() if n.startswith(('UpperArm_','Shoulder_')))
        if removed<1e-5:continue
        # Finger/forearm surfaces are outside the selected rigid chest panels.
        assert not any(n.startswith(('DJ_','LowerArm_')) for n in w)
        weights[i]={n:v for n,v in w.items() if not n.startswith(('UpperArm_','Shoulder_'))};weights[i]['Chest']=weights[i].get('Chest',0)+removed;changed.append(i)
    fixed=copy.deepcopy(s);fixed.skin={}
    for n in study.bind:
        ii=[];ww=[]
        for i,a in enumerate(study.auth):
            w=weights[int(a)].get(n,0)
            if w>0:ii.append(i);ww.append(w)
        if not ii:continue
        ii=np.array(ii);w=np.array(ww);p,q,scale=study.bind[n];fixed.skin[n]=(ii,w,q.inv().apply(fixed.v[ii]-p)/scale)
    return fixed,{'authored_vertices_changed':len(changed),'changed_ids':changed,'selected_panel_faces':len(selected),'frozen_original_collision_face_sets':True,'bind_positions_geometry_topology_hands_unchanged':True},weights

def main():
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);fixed,report,weights=chest_panels(s);out=BASE/'ChestPanelStudy';out.mkdir(exist_ok=True)
    report['before']=s.check(s.base);s.body=fixed;report['after_current_aim']=s.check(s.base)
    for action in ['HoldRifle','AimRifle']:
        p={n:transform(t) for n,t in json.loads((BASE/'NativeRyanCarryStudy'/(action+'-pose.json')).read_text()).items()};report['after_native_'+action]=s.check(p);s.save(out,action,p,report['after_native_'+action])
    (out/'weights.json').write_text(json.dumps(weights));(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='changed_ids'},indent=2))
if __name__=='__main__':main()
