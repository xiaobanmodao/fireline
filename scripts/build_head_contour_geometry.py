"""Local reference-derived shell candidate. Requires existing NumPy/SciPy.

Preserve the collar interface and all original topology/weights/bones. This
constructs geometry only; it does not establish a valid aiming pose.
"""
from pathlib import Path
from collections import Counter
import json,numpy as np
from scipy.spatial import ConvexHull
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved/HeadContourStudy'
d=json.loads((OUT/'mesh-reference.json').read_text());src=d['source'];dst=d['candidate']
sv=np.array(src['vertices']);tv=np.array(dst['vertices']);se=np.array(src['edges']);te=np.array(dst['edges'])
scale=dst['height_cm']/src['height_cm'];sb=np.array(src['bones']['head']);tb=np.array(dst['bones']['Head'])
hi=np.array([i for i,w in enumerate(dst['weights']) if w.get('Head',0)>.5])
si=np.array([i for i,w in enumerate(src['weights']) if w.get('head',0)>.5])
lo,top=tv[hi,2].min(),tv[hi,2].max();slo,stop=sv[si,2].min(),sv[si,2].max()
sw=np.array([w.get('head',0)+w.get('neck_01',0)+w.get('neck_02',0) for w in src['weights']]);se=se[sw[se].max(1)>.25]
tri=np.array(dst['triangles']);mi=np.array(dst['face_materials'])
eyeids=np.unique(np.array(src['triangles'])[np.array(src['face_materials'])==src['materials'].index('M_Als_Main')])
eye=sv[eyeids].mean(0);eye[0]=-abs(sv[eyeids][sv[eyeids,0]<0,0].mean())
eyet=tb+(eye-sb)*scale
under=np.unique(tri[mi==dst['materials'].index('M_Under')])
under=[i for i in under if abs(tv[i,0])<8 and max(dst['weights'][i],key=dst['weights'][i].get) in ['Neck','Head']]
collar=float(tv[under,2].max());upper=((sv[si,:2].max(0)-sv[si,:2].min(0))*scale+1.6)/(tv[hi,:2].max(0)-tv[hi,:2].min(0))
def smooth(t):t=np.clip(t,0,1);return t*t*(3-2*t)
def section(v,e,z):
    a=v[e[:,0]];b=v[e[:,1]]
    mask=(np.minimum(a[:,2],b[:,2])<=z)&(np.maximum(a[:,2],b[:,2])>=z)&(abs(b[:,2]-a[:,2])>1e-7)
    a=a[mask];b=b[mask];p=a[:,:2]+(b[:,:2]-a[:,:2])*((z-a[:,2])/(b[:,2]-a[:,2]))[:,None]
    p=np.unique(p.round(7),axis=0);h=p[ConvexHull(p).vertices]
    cross=h[:,0]*np.roll(h[:,1],-1)-h[:,1]*np.roll(h[:,0],-1)
    center=np.sum((h+np.roll(h,-1,axis=0))*cross[:,None],axis=0)/(3*cross.sum())
    return h,center
def radius(h,u):
    e=np.roll(h,-1,axis=0)-h;den=u[0]*e[:,1]-u[1]*e[:,0]
    q=np.divide(h[:,0]*e[:,1]-h[:,1]*e[:,0],den,out=np.full(len(h),np.inf),where=abs(den)>1e-9)
    t=np.divide(h[:,0]*u[1]-h[:,1]*u[0],den,out=np.full(len(h),np.inf),where=abs(den)>1e-9)
    valid=(q>0)&(t>=-1e-6)&(t<=1+1e-6);assert valid.any();return q[valid].min()
zold=[lo,190.,top];zref=tb[2]+(np.array([slo,eye[2],stop])-sb[2])*scale
new=tv.copy()
for i,p in enumerate(tv):
    if p[2]<=collar or p[2]>top+.01 or abs(p[0])>16 or not -16<=p[1]<=22:continue
    if any(v>.001 and ('Arm' in n or n.startswith(('Shoulder_','DJ_'))) for n,v in dst['weights'][i].items()):continue
    rz=np.clip(sb[2]+(np.interp(p[2],zold,zref)-tb[2])/scale,slo-.05,stop-.05)
    sh,sc=section(sv,se,rz);th,tc=section(tv,te,np.clip(p[2],lo+.05,top-.05))
    v=p[:2]-tc;dist=np.linalg.norm(v)
    if dist<1e-7:continue
    u=v/dist;fraction=min(1.05,dist/radius(th-tc,u))
    shell=.8*smooth((np.interp(p[2],zold,zref)-zref[0])/3)
    xy=tb[:2]+(sc-sb[:2])*scale+u*fraction*(radius(sh-sc,u)*scale+shell)
    b=smooth((p[2]-(lo+.42*(top-lo)))/(.18*(top-lo)))
    xy=(1-b)*xy+b*p[:2]*upper;a=smooth((p[2]-collar)/(190-collar))
    new[i,:2]=(1-a)*p[:2]+a*xy
    new[i,2]=np.interp(p[2],[collar,190.,top],[collar,zref[1],zref[2]])
glass=new[tri[mi==dst['materials'].index('M_Visor')]];hits=[]
for t in glass:
    mat=np.column_stack([(t[1]-t[0])[[0,2]],(t[2]-t[0])[[0,2]]])
    if abs(np.linalg.det(mat))<1e-8:continue
    uv=np.linalg.solve(mat,eyet[[0,2]]-t[0,[0,2]])
    if min(uv)>-1e-6 and sum(uv)<1+1e-6:hits.append(t[0,1]+uv[0]*(t[1,1]-t[0,1])+uv[1]*(t[2,1]-t[0,1]))
assert hits;proxy=[float(eyet[0]),float(max(hits)-1),float(eyet[2])]
area=np.linalg.norm(np.cross(new[tri][:,1]-new[tri][:,0],new[tri][:,2]-new[tri][:,0]),axis=1)/2;assert area.min()>1e-8
edges=Counter(tuple(sorted((int(a),int(b)))) for t in tri for a,b in zip(t,np.roll(t,-1)));assert set(edges.values())=={2}
changed=np.flatnonzero(np.linalg.norm(new-tv,axis=1)>1e-5)
report={'revision':'collar-preserved','source_eye_center_bind_cm':eye.tolist(),'eye_proxy_bind_cm':proxy,
    'collar_identity_band_top_cm':collar,'head_z_map':[[collar,collar],[190.,float(zref[1])],[float(top),float(zref[2])]],
    'changed_vertices':len(changed),'vertices':len(new),'triangles':len(tri),
    'max_displacement_cm':float(np.linalg.norm(new-tv,axis=1).max()),'min_triangle_area_cm2':float(area.min()),
    'native_bind_weights_topology_unchanged':True,'geometry_review_only':True,'static_aim_accepted':False,
    'source_sections':'Native ALS FBX head/neck sections and eyeball material; .8cm shell allowance'}
(OUT/'contour.json').write_text(json.dumps({'vertices_cm':new.tolist(),'before_vertices_cm':tv.tolist(),'changed_vertex_ids':changed.tolist(),'report':report}))
(OUT/'construction.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
