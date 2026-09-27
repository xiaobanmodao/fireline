"""Offline whole-chain source adaptation for the isolated native review.

Requires NumPy/SciPy in Saved/AnimationPython. No uasset, bind or skin writes.
Source = recorded final ALS graph. Target = selected native Ryan/DJ/M4.
"""
from pathlib import Path
import csv, json, struct, argparse
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.optimize import least_squares

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved/WholeCarrySource'
OUT=DATA.parent/'WholeCarryReview'
IDENT=R.identity()

def unit(v):
    n=np.linalg.norm(v)
    if n<1e-7: raise ValueError('Degenerate anatomical direction')
    return v/n
def between(a,b):
    a,b=unit(a),unit(b);d=np.dot(a,b)
    if d<-.99999: raise ValueError('Antiparallel registration')
    return R.from_quat(np.r_[np.cross(a,b),1+d])
def frame(long,across):
    x=unit(long);y=unit(across-x*np.dot(across,x));return R.from_matrix(np.column_stack([x,y,np.cross(x,y)]))
def mixq(a,b,w): return R.from_rotvec((b*a.inv()).as_rotvec()*w)*a
def angle(a,b):return float(np.degrees(np.arccos(np.clip(np.dot(unit(a),unit(b)),-1,1))))
def transform(d):return (np.array(d['p']),R.from_quat(d['q']),np.array(d['s']))
def read_rows(path):
    with path.open(encoding='utf-8-sig') as f:return list(csv.DictReader(f))
def row_tr(r):return (np.array([float(r[k]) for k in ('x','y','z')]),R.from_quat([float(r[k]) for k in ('qx','qy','qz','qw')]),np.array([float(r[k]) for k in ('sx','sy','sz')]))
def local(t,p):return (p[1].inv().apply(t[0]-p[0])/p[2],p[1].inv()*t[1],t[2]/p[2])
def compose(t,p):return (p[0]+p[1].apply(t[0]*p[2]),p[1]*t[1],p[2]*t[2])
def descendant(n,p,parents):
    while n:
        if n==p:return True
        n=parents.get(n)
    return False

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--amplitude',type=float,default=.55);parser.add_argument('--proportions',action='store_true');args=parser.parse_args()
    global OUT
    if args.proportions:OUT=DATA.parent/'BodyProportionReview'
    assert 0<=args.amplitude<=1
    target_path=(DATA.parent/'BodyProportionStudy/target.json') if args.proportions else DATA/'target.json'
    target=json.loads(target_path.read_text());names=target['names'];parents=target['parents']
    B={n:transform(v) for n,v in target['reference'].items()};H={n:transform(v) for n,v in target['hold'].items()}
    srcrows=read_rows(DATA/'bind.csv');SN=[r['bone'] for r in srcrows];SP={r['bone']:r['parent'] if r['parent']!='None' else None for r in srcrows};SB={r['bone']:row_tr(r) for r in srcrows}
    frames={}
    for row in read_rows(DATA/'poses.csv'):frames.setdefault(int(row['frame']),{})[row['bone']]=row_tr(row)
    # Drop the initial overlay selection transition. Keep settled idle, start,
    # complete walking cycles and authored stopping behavior from the live graph.
    states=read_rows(DATA/'states.csv')[60:];source=[frames[int(r['frame'])] for r in states]
    assert len(source)>300
    assert np.ptp([p['foot_l'][0][1] for p in source])>20, 'Frozen source recording'
    S0=source[0]
    BL={n:local(B[n],B[parents[n]]) if parents[n] else B[n] for n in names}
    HL={n:local(H[n],H[parents[n]]) if parents[n] else H[n] for n in names}
    def palm(P,side,native=False):
        key=lambda k:('DJ_'+k+'_'+side.upper()) if native else k+'_'+side
        wrist=key('wrist') if native else 'hand_'+side
        return frame(P[key('middle_01')][0]-P[wrist][0],P[key('index_01')][0]-P[key('pinky_01')][0])
    def deform(S,s):return S[s][1]*SB[s][1].inv()
    lengths={side: [np.linalg.norm(B[b+'_'+side][0]-B[a+'_'+side][0]) for a,b in [('UpperArm','LowerArm'),('LowerArm','DJ_wrist')]] for side in ['L','R']}
    arm_bind_align={}
    for side in ['L','R']:
        s=side.lower();arm_bind_align[side]=between(B['LowerArm_'+side][0]-B['UpperArm_'+side][0],SB['lowerarm_'+s][0]-SB['upperarm_'+s][0])
    # Root translation follows the original stance/gait, scaled by leg length.
    leg_ratio=np.mean([sum(np.linalg.norm(B[b+'_'+s][0]-B[a+'_'+s][0]) for a,b in [('UpperLeg','LowerLeg'),('LowerLeg','Foot')])/sum(np.linalg.norm(SB[b+'_'+s.lower()][0]-SB[a+'_'+s.lower()][0]) for a,b in [('thigh','calf'),('calf','foot')]) for s in ['L','R']])
    def ik(S,E,W,T,l1,l2,phi=0):
        axis=unit(T-S);d=np.linalg.norm(T-S)
        along=(l1*l1-l2*l2+d*d)/(2*d)
        radius=np.sqrt(max(1e-8,l1*l1-along*along))
        pole=unit(E-S-axis*np.dot(E-S,axis))
        pole=R.from_rotvec(axis*phi).apply(pole)
        return S+axis*along+pole*radius,l1+l2-d
    raw=[]
    for S in source:
        P={}
        mapped={'Pelvis':'pelvis','Torso':'spine_01','Chest':'spine_03','Neck':'neck_01','Head':'head'}
        for n in names:
            parent=parents[n]
            P[n]=compose(BL[n],P[parent]) if parent else B[n]
            if n in mapped:
                q=deform(S,mapped[n])*B[n][1]
                pos=P[n][0]
                if n=='Pelvis':pos=B[n][0]+(S['pelvis'][0]-SB['pelvis'][0])*leg_ratio
                P[n]=(pos,q,B[n][2])
            for side in ['L','R']:
                s=side.lower();sh='Shoulder_'+side;u='UpperArm_'+side;e='LowerArm_'+side;w='DJ_wrist_'+side
                if n==sh:
                    q=deform(S,'clavicle_'+s)*between(B[u][0]-B[sh][0],SB['upperarm_'+s][0]-SB['clavicle_'+s][0])*B[sh][1]
                    P[n]=(P[n][0],q,B[n][2])
                if n==u:
                    P[n]=(P[n][0],deform(S,'upperarm_'+s)*arm_bind_align[side]*B[n][1],B[n][2])
                if n==e:
                    v=unit(S['hand_'+s][0]-S['lowerarm_'+s][0])
                    dq=deform(S,'lowerarm_'+s)*between(B[w][0]-B[e][0],SB['hand_'+s][0]-SB['lowerarm_'+s][0])
                    P[n]=(P[n][0],dq*B[n][1],B[n][2])
                if n=='DJ_forearm_'+side:
                    P[n]=compose(local(B[n],B[e]),P[e])
                if n==w:
                    pos=P[e][0]+unit(S['hand_'+s][0]-S['lowerarm_'+s][0])*lengths[side][1]
                    q=palm(S,s)*palm(H,side,True).inv()*H[n][1]
                    P[n]=(pos,q,H[n][2])
                elif descendant(n,w,parents):P[n]=compose(HL[n],P[parent])
                if n=='UpperLeg_'+side:
                    q=deform(S,'thigh_'+s)*between(B['LowerLeg_'+side][0]-B[n][0],SB['calf_'+s][0]-SB['thigh_'+s][0])*B[n][1]
                    P[n]=(P[n][0],q,B[n][2])
                if n=='LowerLeg_'+side:
                    q=deform(S,'calf_'+s)*between(B['Foot_'+side][0]-B[n][0],SB['foot_'+s][0]-SB['calf_'+s][0])*B[n][1]
                    P[n]=(P[n][0],q,B[n][2])
                if n=='Foot_'+side:P[n]=(P[n][0],deform(S,'foot_'+s)*B[n][1],B[n][2])
        # Keep all mechanics and glove local transforms from the selected hold.
        for n in names:
            if n.startswith('M4_'):P[n]=compose(local(H[n],H['DJ_wrist_R']),P['DJ_wrist_R'])
        raw.append(P)
    # Register floor height once, leaving source pelvis motion intact.
    floor_shift=np.mean([B['Foot_'+s][0][2]-raw[0]['Foot_'+s][0][2] for s in ['L','R']])
    for P in raw:
        for n in names:
            if n not in ['RyanRig','Root']:P[n]=(P[n][0]+np.array([0,0,floor_shift]),P[n][1],P[n][2])
    # Preserve source ankle trajectories in native hip reference space. A foot-
    # reference registration is wrong here: ALS and Ryan place their ankle bones
    # differently relative to the hip even in the bind pose. One common leg
    # scale drives both these trajectories and character travel in the viewer.
    leg_metrics=[]
    for P,S in zip(raw,source):
        row=[]
        for side in ['L','R']:
            u,e,f=['UpperLeg_'+side,'LowerLeg_'+side,'Foot_'+side]
            hip,knee,ankle=[P[n][0] for n in [u,e,f]]
            target=B[u][0]+(S['foot_'+side.lower()][0]-SB['thigh_'+side.lower()][0])*leg_ratio+np.array([0,0,floor_shift])
            l1=np.linalg.norm(B[e][0]-B[u][0]);l2=np.linalg.norm(B[f][0]-B[e][0])
            nk,margin=ik(hip,knee,ankle,target,l1,l2)
            if margin<0:raise ValueError('Unreachable mapped source ankle; revise reference registration')
            P[u]=(hip,between(knee-hip,nk-hip)*P[u][1],P[u][2])
            P[e]=(nk,between(ankle-knee,target-nk)*P[e][1],P[e][2])
            shift=target-ankle
            for n in names:
                if descendant(n,f,parents):P[n]=(P[n][0]+shift,P[n][1],P[n][2])
            row.append({'reach_margin':float(margin),'correction_cm':float(np.linalg.norm(shift))})
        leg_metrics.append(row)
    hmid=(H['DJ_wrist_L'][0]+H['DJ_wrist_R'][0])*.5;hspan=H['DJ_wrist_L'][0]-H['DJ_wrist_R'][0]
    def assembly(P):
        q=P['DJ_wrist_R'][1]*H['DJ_wrist_R'][1].inv()
        q=between(q.apply(hspan),P['DJ_wrist_L'][0]-P['DJ_wrist_R'][0])*q
        center=(P['DJ_wrist_L'][0]+P['DJ_wrist_R'][0])*.5
        chest=P['Chest'][1]*B['Chest'][1].inv()
        return chest.inv()*q,chest.inv().apply(center-P['Chest'][0]),chest
    qa,ca,_=assembly(raw[0]);cache=[]
    for P in raw:
        q,c,chest=assembly(P)
        cache.append((P,chest*mixq(qa,q,args.amplitude),P['Chest'][0]+chest.apply(ca+(c-ca)*args.amplitude),chest))
    def solve(x,entry,build=False):
        P,q,c,chest=entry;q=chest*R.from_rotvec(x[:3])*chest.inv()*q;c=c+chest.apply(x[3:6]);rows=[];arms=[];N=dict(P)
        transfer=lambda n:(c+q.apply(H[n][0]-hmid),q*H[n][1],H[n][2])
        for i,side in enumerate(['L','R']):
            u,e,w=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];S,E,W=[P[n][0] for n in [u,e,w]];T=transfer(w);l1,l2=lengths[side]
            NE,margin=ik(S,E,W,T[0],l1,l2,x[6+i]);fore=unit(T[0]-NE)
            hand_d=T[1]*B[w][1].inv();natural=hand_d.apply(unit(B[w][0]-B[e][0]));bend=angle(natural,fore)
            rows.extend((T[0]-W)/8);rows.extend((NE-E)/8)
            rows.append(max(0,bend-25)/3);rows.append(max(0,2-margin)*3)
            # Source guide is the moving elbow plane, not a universal down pole.
            local_elbow=chest.inv().apply(NE-P['Chest'][0]);local_wrist=chest.inv().apply(T[0]-P['Chest'][0])
            def inside(V):return max(0,1-np.sqrt((V[0]/(20 if args.proportions else 23))**2+(V[1]/(15 if args.proportions else 16))**2))
            rows.append(inside(local_elbow)*8);rows.append(inside(local_wrist)*8)
            arms.append({'wrist_bend':bend,'reach_margin':margin,'elbow_shift':float(np.linalg.norm(NE-E)),'wrist_shift':float(np.linalg.norm(T[0]-W))})
            if build:
                N[u]=(S,between(E-S,NE-S)*P[u][1],P[u][2])
                dq=between(natural,fore)*hand_d
                for n in [e,'DJ_forearm_'+side]:N[n]=(NE+dq.apply(B[n][0]-B[e][0]),dq*B[n][1],B[n][2])
                for n in names:
                    if descendant(n,w,parents):N[n]=transfer(n)
        if build:
            for n in names:
                if n.startswith('M4_'):N[n]=transfer(n)
        return rows,arms,N
    subset=cache[::12]
    if args.proportions:
        from body_proportion_geometry import Surface,chest_hull,signed
        geometry=DATA.parent/'BodyProportionStudy'
        surface=Surface(geometry/'candidate')
        torso_surface=surface.hull_samples(lambda n:n in ['Chest','Torso'])
        arm_surface=surface.subset(np.flatnonzero([n.startswith('LowerArm_') or n.startswith('DJ_') or (n.startswith('UpperArm_') and np.linalg.norm(v-B[n][0])>9) for n,v in zip(surface.dom,surface.v)]))
        gun_surface=Surface(geometry/'gun')
        clearance_hulls=[chest_hull(torso_surface,entry[0]) for entry in subset]
    def objective(x):
        rows=[]
        for i,entry in enumerate(subset):
            residual,_,N=solve(x,entry,args.proportions);rows.extend(residual)
            if args.proportions:
                # Collision uses the actual skinned Ryan torso envelope and
                # forearm/glove/weapon surface, not only two wrist endpoints.
                for shape in [arm_surface,gun_surface]:
                    distances=signed(shape.deform(N),clearance_hulls[i])
                    rows.extend(np.maximum(0,1.0-distances)*5)
        rows.extend(x[:3]/.6);rows.extend(x[3:6]/20);rows.extend(x[6:]/.7)
        return rows
    bound=np.r_[np.ones(3)*.85,np.ones(3)*20,np.ones(2)*.85]
    initial=np.zeros(8)
    # Fixed seed from the first surface registration; reproducible rather than
    # silently warm-starting from whatever report happens to be on disk.
    if args.proportions:initial=np.array([-.4368922740665897,-.7138369328916181,.7865772428724283,4.221535434233251,7.134850714513233,-2.2891596860844734,.10811100240537985,.03197974449375853])
    fit=least_squares(objective,initial,bounds=(-bound,bound),max_nfev=100)
    final=[];metrics=[]
    for entry in cache:
        _,m,P=solve(fit.x,entry,True);final.append(P);metrics.append(m)
    OUT.mkdir(exist_ok=True)
    (OUT/'leg-scale.txt').write_text(str(leg_ratio))
    def output(name,poses,bones,ps):
        # Engine preview consumes local transforms; scales stay exactly native.
        with (OUT/(name+'.bin')).open('wb') as f:
            f.write(struct.pack('<III',0x46574331,len(bones),len(poses)))
            for P in poses:
                for n in bones:
                    tr=local(P[n],P[ps[n]]) if ps.get(n) else P[n]
                    f.write(struct.pack('<10f',*tr[0],*tr[1].as_quat(),*tr[2]))
        (OUT/(name+'-bones.txt')).write_text('\n'.join(bones))
    output('source',source,SN,SP);output('retarget',raw,names,parents);output('contact',final,names,parents)
    if args.proportions:
        import shutil
        for ext in ['.bin','-bones.txt']:shutil.copyfile(DATA.parent/'WholeCarryReview'/('contact'+ext),OUT/('retarget'+ext))
    np.array([[float(s[k]) for k in ['speed','actor_x','actor_y','actor_z']] for s in states],dtype='<f4').tofile(OUT/'movement.bin')
    summary={'frames':len(source),'fps':60,'amplitude_candidate':args.amplitude,'fit':fit.x.tolist(),'fit_converged':bool(fit.success),'fit_initial':initial.tolist(),'fit_evaluations':int(fit.nfev),
             'native_assets_modified':False,'derived_proportion_mesh':args.proportions,'visual_acceptance':False,'leg_scale':leg_ratio,
             'legs':{side:{k:{'min':min(m[i][k] for m in leg_metrics),'max':max(m[i][k] for m in leg_metrics)} for k in leg_metrics[0][i]} for i,side in enumerate(['L','R'])},
             'arms':{side:{k:{'min':min(m[i][k] for m in metrics),'max':max(m[i][k] for m in metrics)} for k in metrics[0][i]} for i,side in enumerate(['L','R'])}}
    (OUT/'report.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
    # Diagnostic pose values retained locally for exact frame inspection.
    import gzip
    (OUT/'poses.json.gz').write_bytes(gzip.compress(json.dumps({'raw':[{n:{'p':t[0].tolist(),'q':t[1].as_quat().tolist(),'s':t[2].tolist()} for n,t in P.items()} for P in raw],
        'contact':[{n:{'p':t[0].tolist(),'q':t[1].as_quat().tolist(),'s':t[2].tolist()} for n,t in P.items()} for P in final]}).encode(),mtime=0))

if __name__=='__main__':main()
