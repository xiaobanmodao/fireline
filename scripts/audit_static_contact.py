"""Check the imported weight-only candidate using native render buffers."""
import csv,json,argparse,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
from body_proportion_geometry import Surface,rows
from study_surface_mount import SurfaceMount,BASE,REF,transform,geo

def point_triangle(points,tri):
    best=float('inf')
    for p in points:
        a=tri[:,0];ab=tri[:,1]-a;ac=tri[:,2]-a;ap=p-a
        d00=np.sum(ab*ab,1);d01=np.sum(ab*ac,1);d11=np.sum(ac*ac,1)
        den=d00*d11-d01*d01;valid=den>1e-12
        d20=np.sum(ap*ab,1);d21=np.sum(ap*ac,1)
        v=np.divide(d11*d20-d01*d21,den,out=np.zeros(len(tri)),where=valid)
        w=np.divide(d00*d21-d01*d20,den,out=np.zeros(len(tri)),where=valid)
        inside=valid&(v>=0)&(w>=0)&(v+w<=1)
        dist=np.where(inside,np.linalg.norm(a+v[:,None]*ab+w[:,None]*ac-p,axis=1),np.inf)
        for j in range(3):
            aa=tri[:,j];ee=tri[:,(j+1)%3]-aa;length=np.sum(ee*ee,1)
            t=np.clip(np.divide(np.sum((p-aa)*ee,1),length,out=np.zeros(len(tri)),where=length>1e-12),0,1)
            dist=np.minimum(dist,np.linalg.norm(aa+t[:,None]*ee-p,axis=1))
        best=min(best,float(dist.min()))
    return best

def distance(a,b):
    if len(geo.triangle_crossings(a,b)):return 0.
    nearest=min(point_triangle(a.reshape(-1,3),b),point_triangle(b.reshape(-1,3),a))
    ae=np.concatenate([a[:,[0,1]],a[:,[1,2]],a[:,[2,0]]]);be=np.concatenate([b[:,[0,1]],b[:,[1,2]],b[:,[2,0]]])
    for edge in ae:
        u=edge[1]-edge[0];v=be[:,1]-be[:,0];w=edge[0]-be[:,0]
        A=np.dot(u,u);B=np.sum(v*u,1);C=np.sum(v*v,1);D=np.sum(w*u,1);E=np.sum(v*w,1);den=A*C-B*B;valid=den>1e-10
        ss=np.divide(B*E-C*D,den,out=np.zeros(len(be)),where=valid);tt=np.divide(A*E-B*D,den,out=np.zeros(len(be)),where=valid)
        valid&=(ss>=0)&(ss<=1)&(tt>=0)&(tt<=1)
        if valid.any():nearest=min(nearest,float(np.linalg.norm(w[valid]+ss[valid,None]*u-tt[valid,None]*v[valid],axis=1).min()))
    return nearest

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--run');args=parser.parse_args()
    out=REF.parent/'ChestPanelStudy';s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90)
    native=Surface(out/'candidate');old=s.body
    assert np.array_equal(old.tris,native.tris),'Native triangle order changed; remap frozen face audit before continuing'
    assert np.max(np.linalg.norm(old.v-native.v,axis=1))<1e-5
    oldbind=rows(REF/'candidate.bind.csv');newbind=rows(out/'candidate.bind.csv')
    assert len(oldbind)==len(newbind)==131
    for a,b in zip(oldbind,newbind):
        assert a['bone']==b['bone'] and a['parent']==b['parent']
        assert max(abs(float(a[k])-float(b[k])) for k in ['x','y','z','qx','qy','qz','qw','sx','sy','sz'])<1e-5
    def groups(path):
        g={}
        for r in rows(path):g.setdefault(int(r['vertex']),{})[r['bone']]=int(r['weight'])
        return g
    ow=groups(REF/'candidate.weights.csv');nw=groups(out/'candidate.weights.csv')
    changed=[i for i in ow if ow[i]!=nw[i]]
    expected=set(json.loads((BASE/'ChestPanelStudy/report.json').read_text())['changed_ids'])
    assert set(s.auth[changed])==expected
    assert not any(n.startswith('DJ_') for i in changed for n in set(ow[i])|set(nw[i]))
    # Keep the ORIGINAL semantic face sets and source vertex adjacency after reweighting.
    s.body=native
    pose={n:transform(v) for n,v in json.loads((BASE/'NativeStockSeatStudy/AimRifle-pose.json').read_text()).items()}
    report={'native_render_vertices':len(native.v),'triangles':len(native.tris),'native_bind_bones':131,'geometry_unchanged':True,'bind_unchanged':True,'changed_native_vertices':len(changed),'changed_authored_vertices':len(expected),'hand_weights_unchanged':True}
    report.update(s.check(pose))
    bv=native.deform(pose);gv=s.gun.deform(pose);held=s.gun.deform(s.hold)
    # The actual rear pad is the minimum forward coordinate face, rather than a broad stock box.
    # The buttpad is raked, not a plane normal to the barrel. This 2cm rear
    # surface band includes its complete bottom and top instead of a single tip.
    pad=np.flatnonzero((held[s.gun.tris][:,:,1].max(1)-held[:,1].min())<2.)
    shoulder=np.flatnonzero(np.isin(old.dom[old.tris],['Chest','Shoulder_R','UpperArm_R']).any(1))
    assert len(pad)>0
    upperarm=np.flatnonzero(np.isin(old.dom[old.tris],['UpperArm_L','UpperArm_R']).any(1))
    report['upperarm_gun_pairs']=len(geo.triangle_crossings(bv[native.tris[upperarm]],gv[s.gun.tris]))
    report.update(rear_pad_faces=len(pad),rear_pad_shoulder_distance_cm=distance(gv[s.gun.tris[pad]],bv[native.tris[shoulder]]),distance_method='All vertex-face and interior edge-edge candidates plus triangle crossings')
    tri=bv[native.tris];padv=gv[np.unique(s.gun.tris[pad])];votes=[]
    for direction in [[1,.123,.287],[.133,1,.281],[.157,.231,1]]:
        direction=np.array(direction);direction/=np.linalg.norm(direction);edge1=tri[:,1]-tri[:,0];edge2=tri[:,2]-tri[:,0]
        h=np.cross(direction,edge2);det=np.sum(edge1*h,1);valid=np.abs(det)>1e-9;inv=np.divide(1.,det,out=np.zeros(len(det)),where=valid);counts=[]
        for point in padv:
            delta=point-tri[:,0];u=inv*np.sum(delta*h,1);q=np.cross(delta,edge1);v=inv*np.sum(q*direction,1);t=inv*np.sum(edge2*q,1)
            hit=valid&(u>=0)&(v>=0)&(u+v<=1)&(t>1e-5);counts.append(len(np.unique(np.round(t[hit],5)))%2)
        votes.append(sum(counts))
    report['rear_pad_inside_body_votes']=votes
    if args.run:
        from study_surface_mount import load_pose
        run=BASE/'LiveCarryProject/Saved/LiveAim'/args.run
        for frame in [0,90,149]:
            actual=load_pose(run/'poses.csv',frame)
            assert set(actual)==set(pose)
            assert max(np.linalg.norm(actual[n][0]-pose[n][0]) for n in pose)<2e-4
            assert max((actual[n][1]*pose[n][1].inv()).magnitude() for n in pose)<1e-5
        report['native_component_pose_export_parity']=True
    report.update(runtime_default_enabled=False,visual_acceptance=False,moving_hold_passed=False,eye_registration_finished=False)
    (out/'native-audit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
