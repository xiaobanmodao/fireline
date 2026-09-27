"""Actual skinned surface checks for the isolated M4 proportion derivative.

The convex torso envelope is conservative. Clip suspect triangles against all
of its planes; vertex-only tests can miss an edge cutting through the chest.
"""
from pathlib import Path
import json,gzip,csv
import numpy as np
from scipy.spatial import ConvexHull,cKDTree
from scipy.spatial.transform import Rotation as R
from body_proportion_geometry import Surface,rows,chest_hull,signed
ROOT=Path(__file__).resolve().parents[1];D=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved';G=D/'BodyProportionStudy';OUT=D/'BodyProportionReview'
body=Surface(G/'candidate');gun=Surface(G/'gun');poses=json.loads(gzip.decompress((OUT/'poses.json.gz').read_bytes()))['contact']
bind=json.loads((G/'target.json').read_text())['reference'];torso=body.subset(np.flatnonzero(np.isin(body.dom,['Chest','Torso'])))
armmask=np.array([n.startswith('LowerArm_') or n.startswith('DJ_') or (n.startswith('UpperArm_') and np.linalg.norm(p-np.array(bind[n]['p']))>9) for n,p in zip(body.dom,body.v)])
armtris=body.tris[armmask[body.tris].all(axis=1)]
def clipped_triangle(tri,planes):
 poly=list(tri)
 for plane in planes:
  if not poly:return []
  new=[];prev=poly[-1];pd=np.dot(prev,plane[:3])+plane[3]
  for point in poly:
   d=np.dot(point,plane[:3])+plane[3]
   if (d<=0)!=(pd<=0):new.append(prev+(point-prev)*(pd/(pd-d)))
   if d<=0:new.append(point)
   prev,pd=point,d
  poly=new
 return poly
clearance={'arms':1e9,'gun':1e9};intersections=[];max_suspects=0
for i,P in enumerate(poses):
 V=body.deform(P);h=ConvexHull(V[np.isin(body.dom,['Chest','Torso'])]).equations
 for label,verts,tris,mask in [('arms',V,armtris,armmask),('gun',gun.deform(P),gun.tris,np.ones(len(gun.v),bool))]:
  dots=np.einsum('ij,kj->ik',verts,h[:,:3],optimize=False)+h[:,3]
  clearance[label]=min(clearance[label],float(dots[mask].max(axis=1).min()))
  candidates=np.flatnonzero(dots[tris].min(axis=1).max(axis=1)<0)
  max_suspects=max(max_suspects,len(candidates))
  for face in candidates:
   clipped=clipped_triangle(verts[tris[face]],h)
   if len(clipped)>=3:
    area=sum(np.linalg.norm(np.cross(clipped[j]-clipped[0],clipped[j+1]-clipped[0]))*.5 for j in range(1,len(clipped)-1))
    if area>1e-4:intersections.append({'frame':i,'surface':label,'face':int(face),'area_cm2':float(area)})
# Count true topology after collapsing only coincident render splits.
_,remap=np.unique(body.v.round(4),axis=0,return_inverse=True);tris=remap[body.tris];edges=np.sort(np.concatenate([tris[:,[0,1]],tris[:,[1,2]],tris[:,[2,0]]]),axis=1);ue,counts=np.unique(edges,axis=0,return_counts=True)
parent=np.arange(remap.max()+1)
def root(i):
 while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
 return i
for a,b in ue:parent[root(a)]=root(b)
# Hand/finger rest surface must be a rigid inward translation, not reshaping.
oldv=np.array([[float(r[k]) for k in ['x','y','z']] for r in rows(D/'WholeCarryReview/native-bind-vertices.csv')]);translations=json.loads((G/'construction.json').read_text())['bind_translations_cm'];hand_errors=[]
for side in ['L','R']:
 ids=np.flatnonzero([n.startswith('DJ_') and n.endswith('_'+side) and 'forearm' not in n for n in body.dom]);q=body.v[ids]-translations['DJ_wrist_'+side];distance,_=cKDTree(oldv).query(q);hand_errors.extend(distance.tolist())
report={'frames':len(poses),'torso_test':'actual deformed Chest/Torso convex hull; triangle half-space clipping','forearm_glove_and_distal_upperarm_faces':len(armtris),'weapon_faces':len(gun.tris),'minimum_vertex_separation_cm':clearance,'intersecting_triangles':len(intersections),'max_broadphase_suspects_per_frame':max_suspects,'welded_components':len({root(i) for i in range(len(parent))}),'boundary_edges':int((counts==1).sum()),'nonmanifold_edges':int((counts>2).sum()),'native_hand_surface_translation_error_cm_max':max(hand_errors),'visual_acceptance':False,'excluded':'proximal upper arm within 9cm of shoulder is an attached anatomical junction; this is not a universal self-intersection or all-animation audit'}
(OUT/'surface-audit.json').write_text(json.dumps(report,indent=2));(OUT/'triangle-intersections.json').write_text(json.dumps(intersections,indent=2));print(json.dumps(report,indent=2))
assert not intersections,'Actual triangles enter the conservative torso envelope'
assert report['welded_components']==1 and report['boundary_edges']==0 and report['nonmanifold_edges']==0
assert report['native_hand_surface_translation_error_cm_max']<.02
