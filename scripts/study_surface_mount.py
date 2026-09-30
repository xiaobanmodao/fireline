"""Single captured-pose contact diagnosis; no runtime or asset writes.

Use the real render surfaces, original helmet, and the native rigid M4/grip.
The experiment registers the sight line to the existing design eye marker;
this does not establish that marker's anatomical correctness.
"""
from pathlib import Path
import argparse,csv,json
import numpy as np
from scipy.spatial import cKDTree
from scipy.spatial.transform import Rotation as R
from body_proportion_geometry import Surface
from build_whole_carry_candidate import transform,unit,between,descendant

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch'
REF=BASE/'ReferenceProject/Saved/BodyProportionStudy'
import mesh_contact_geometry as geo

def escape_along(a,b,direction,margin=.15,first_contact=False):
    """First collision-free translation of b along a fixed direction.

    Solve continuous SAT overlap intervals, then merge the component containing
    zero. This is a triangle sweep, not a sampled offset/point-fit search.
    """
    x=unit(np.cross(direction,[0,0,1]));z=unit(np.cross(x,direction));basis=np.column_stack([x,direction,z]);aa=a@basis;bb=b@basis
    amin,amax=aa[:,:,[0,2]].min(1),aa[:,:,[0,2]].max(1);bmin,bmax=bb[:,:,[0,2]].min(1),bb[:,:,[0,2]].max(1)
    pairs=[]
    for i in range(len(a)):
        pairs.extend((i,int(j)) for j in np.flatnonzero(((bmax>=amin[i]-1e-6)&(bmin<=amax[i]+1e-6)).all(1)))
    pairs=np.array(pairs);aa=a[pairs[:,0]];bb=b[pairs[:,1]];ea=np.roll(aa,-1,axis=1)-aa;eb=np.roll(bb,-1,axis=1)-bb;na=np.cross(ea[:,0],ea[:,1]);nb=np.cross(eb[:,0],eb[:,1]);axes=np.concatenate([na[:,None],nb[:,None],np.cross(ea[:,:,None],eb[:,None]).reshape(-1,9,3),np.cross(na[:,None],ea),np.cross(nb[:,None],eb)],1);norm=np.linalg.norm(axes,axis=2);axes/=np.maximum(norm[:,:,None],1e-20)
    pa=np.einsum('nkj,nij->nki',axes,aa);pb=np.einsum('nkj,nij->nki',axes,bb);v=axes@direction;moving=np.abs(v)>1e-9;valid=(norm>1e-9)
    lo0=pa.min(2)-pb.max(2);hi0=pa.max(2)-pb.min(2);den=np.where(moving,v,1);l=lo0/den;h=hi0/den
    low=np.where(valid&moving,np.minimum(l,h),-np.inf).max(1);high=np.where(valid&moving,np.maximum(l,h),np.inf).min(1)
    separated=(valid&~moving&((lo0>1e-6)|(hi0< -1e-6))).any(1)
    intervals=sorted((float(l),float(h)) for l,h,bad in zip(low,high,separated) if not bad and l<=h and h>=0)
    if first_contact:
        return max(0.,intervals[0][0]) if intervals else float('inf')
    end=0.;found=False
    for lo,hi in intervals:
        if lo>end+1e-6:break
        found=True;end=max(end,hi)
    return end+margin if found else 0.

def load_pose(path,frame):
    with path.open() as f:
        return {r['bone']:(np.array([float(r[k]) for k in ['x','y','z']]),R.from_quat([float(r[k]) for k in ['qx','qy','qz','qw']]),np.array([float(r[k]) for k in ['sx','sy','sz']])) for r in csv.DictReader(f) if int(r['frame'])==frame}

class SurfaceMount:
    def __init__(self,run,frame):
        self.run=run;self.frame=frame;self.base=load_pose(run/'poses.csv',frame)
        d=json.loads((REF/'target.json').read_text());self.bind={n:transform(v) for n,v in d['reference'].items()};self.hold={n:transform(v) for n,v in d['hold'].items()};self.parents=d['parents'];self.names=d['names']
        self.body=Surface(REF/'candidate');self.gun=Surface(REF/'gun');self.q=self.base['M4_body'][1]*self.hold['M4_body'][1].inv();self.pivot=self.hold['M4_body'][0]
        self.eye=self.base['Head'][0]+self.base['Head'][1].apply(self.bind['Head'][1].inv().apply(np.array([-3.2,16,190.])-self.bind['Head'][0]))
        with (run/'optic.csv').open() as f:
            row=list(csv.DictReader(f))[frame];self.sight=np.array([float(row['sight_'+k]) for k in 'xyz']);self.forward=unit(np.array([float(row['axis_'+k]) for k in 'xyz']))
        author=np.array(json.loads((REF.parent/'HeadContourStudy/mesh-reference.json').read_text())['candidate']['vertices']);error,self.auth=cKDTree(author).query(self.body.v);assert error.max()<1e-3
        self.head=np.flatnonzero(np.isin(self.body.dom[self.body.tris],['Head','Neck']).any(1))
        self.arm=np.flatnonzero(np.array([any(n.startswith(('UpperArm_','LowerArm_','DJ_')) for n in self.body.dom[t]) for t in self.body.tris]))
        self.torso=np.flatnonzero(np.isin(self.body.dom[self.body.tris],['Chest','Torso']).any(1)&~np.array([any(n.startswith(('UpperArm_','LowerArm_','DJ_')) for n in self.body.dom[t]) for t in self.body.tris]))
        self.chest=np.flatnonzero(np.isin(self.body.dom[self.body.tris],['Chest','Torso','Shoulder_R','Shoulder_L']).any(1))
        self.held=[n for n in self.names if n.startswith('M4_') or any(descendant(n,'DJ_wrist_'+s,self.parents) for s in ['L','R'])]

    def mount(self,delta,arc_offsets=None):
        p=dict(self.base);center=self.base['M4_body'][0]+delta;wrists=[];reach=[];arcs=[];clavicles=[]
        for n in self.held:
            h=self.hold[n];p[n]=(center+self.q.apply(h[0]-self.pivot),self.q*h[1],h[2])
        for side in ['L','R']:
            u,e,w=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];s,guide=self.base[u][0],self.base[e][0];end=p[w][0]
            a=np.linalg.norm(self.bind[e][0]-self.bind[u][0]);b=np.linalg.norm(self.bind[w][0]-self.bind[e][0]);d=np.linalg.norm(end-s)
            angle=0.
            if d>=a+b-1:
                sh='Shoulder_'+side;pivot=p[sh][0];link=s-pivot;goal=end-pivot;length=np.linalg.norm(link);dist=np.linalg.norm(goal);c=(dist*dist+length*length-(a+b-1)**2)/(2*dist*length)
                if c>1:raise ValueError('Unreachable complete clavicle/arm '+side)
                oldangle=np.arccos(np.clip(np.dot(unit(link),unit(goal)),-1,1));angle=max(0.,oldangle-np.arccos(np.clip(c,-1,1)))
                if angle>np.radians(30):raise ValueError('Clavicle correction exceeds bounded study '+side)
                q=R.from_rotvec(unit(np.cross(link,goal))*angle)
                for n in self.names:
                    if descendant(n,sh,self.parents) and n not in self.held:
                        v,r,scale=p[n];p[n]=(pivot+q.apply(v-pivot),q*r,scale)
                s,guide=p[u][0],p[e][0];d=np.linalg.norm(end-s)
            clavicles.append(float(np.degrees(angle)))
            if d>=a+b or d<=abs(a-b):raise ValueError('Unreachable '+side)
            axis=unit(end-s);along=(a*a-b*b+d*d)/(2*d);radius=np.sqrt(a*a-along*along);pole=unit(guide-s-axis*np.dot(guide-s,axis))
            hand=p[w][1]*self.bind[w][1].inv();natural=hand.apply(unit(self.bind[w][0]-self.bind[e][0]))
            # Preserve the authored elbow plane when its wrist is feasible.
            # Otherwise solve the closest feasible arc analytically, rather
            # than minimizing wrist bend at the expense of the entire shoulder.
            tangent=unit(np.cross(axis,pole));nn=np.array([np.dot(natural,pole),np.dot(natural,tangent)]);amp=np.linalg.norm(nn)
            k=np.dot(natural,axis)*(d-along)/b;limit=np.cos(np.radians(40));arc=0.
            if k-radius/b*nn[0]<limit:
                v=(k-limit)*b/(radius*amp)
                if v< -1-1e-8:raise ValueError('No wrist-feasible elbow arc '+side)
                theta=np.arccos(np.clip(v,-1,1));phi=np.arctan2(nn[1],nn[0]);choices=[(x+np.pi)%(2*np.pi)-np.pi for x in [phi-theta,phi+theta]];arc=min(choices,key=abs)
            if arc_offsets:arc+=np.radians(arc_offsets.get(side,0))
            best=pole*np.cos(arc)+tangent*np.sin(arc);ne=s+along*axis+radius*best
            dq=between(natural,end-ne)*hand
            p[u]=(s,between(guide-s,ne-s)*p[u][1],p[u][2])
            for n in [e,'DJ_forearm_'+side]:p[n]=(ne+dq.apply(self.bind[n][0]-self.bind[e][0]),dq*self.bind[n][1],self.bind[n][2])
            wrists.append(float(np.degrees(np.arccos(np.clip(np.dot(natural,unit(end-ne)),-1,1)))));reach.append(float(a+b-d));arcs.append(float(np.degrees(arc)))
        return p,{'wrist_deg':wrists,'reach_cm':reach,'elbow_arc_from_source_deg':arcs,'clavicle_reach_correction_deg':clavicles}

    def check(self,p):
        bv=self.body.deform(p);gv=self.gun.deform(p)
        head=geo.triangle_crossings(bv[self.body.tris[self.head]],gv[self.gun.tris]);chest=geo.triangle_crossings(bv[self.body.tris[self.chest]],gv[self.gun.tris]);pairs=geo.triangle_crossings(bv[self.body.tris[self.arm]],bv[self.body.tris[self.torso]])
        pairs=[(int(self.arm[i]),int(self.torso[j])) for i,j in pairs if not set(self.auth[self.body.tris[self.arm[i]]])&set(self.auth[self.body.tris[self.torso[j]]])]
        other=np.setdiff1d(np.arange(len(self.body.tris)),self.head);hb=geo.triangle_crossings(bv[self.body.tris[self.head]],bv[self.body.tris[other]])
        hb=[(int(self.head[i]),int(other[j])) for i,j in hb if not set(self.auth[self.body.tris[self.head[i]]])&set(self.auth[self.body.tris[other[j]]])]
        return {'head_gun_pairs':len(head),'chest_gun_pairs':len(chest),'arm_torso_pairs':len(pairs),'arm_torso_faces':pairs,'head_body_pairs':len(hb)}

    def save(self,out,label,p,metrics):
        out.mkdir(parents=True,exist_ok=True)
        (out/(label+'-pose.json')).write_text(json.dumps({n:{'p':t[0].tolist(),'q':t[1].as_quat().tolist(),'s':t[2].tolist()} for n,t in p.items()}))
        for name,surface in [('body',self.body),('gun',self.gun)]:
            v=surface.deform(p);(out/(label+'-'+name+'.obj')).write_text(''.join('v '+' '.join(map(str,x))+'\n' for x in v)+''.join('f '+' '.join(str(i+1) for i in t)+'\n' for t in surface.tris))
        (out/(label+'-report.json')).write_text(json.dumps(metrics,indent=2));print(label,json.dumps(metrics),flush=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--frame',type=int,default=90);ap.add_argument('--forward-cm',type=float,default=0);ap.add_argument('--surface',action='store_true');a=ap.parse_args()
    s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',a.frame)
    out=BASE/'SurfaceMountStudy';s.save(out,'original-'+str(a.frame),s.base,s.check(s.base))
    if a.surface:
        bv=s.body.deform(s.base);gv=s.gun.deform(s.base);faces=np.unique(np.r_[s.head,s.chest]);distance=escape_along(bv[s.body.tris[faces]],gv[s.gun.tris],s.forward);delta=s.forward*distance;print('surface sweep distance',distance,flush=True)
    else:
        delta=s.eye-s.sight;delta-=s.forward*np.dot(delta,s.forward);delta+=s.forward*a.forward_cm
    p,m=s.mount(delta);m.update(s.check(p));m.update({'delta_cm':delta.tolist(),'eye_proxy_cm':s.eye.tolist(),'source_frame':a.frame,'runtime_enabled':False,'visual_acceptance':False})
    s.save(out,('surface-mounted-' if a.surface else 'sight-mounted-')+str(a.frame),p,m)
if __name__=='__main__':main()
