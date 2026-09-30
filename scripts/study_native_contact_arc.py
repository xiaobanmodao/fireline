"""Bounded outward-elbow surface root from the provider's authored plane."""
import json,argparse,numpy as np
from study_surface_mount import SurfaceMount,BASE,REF,transform,unit,geo,load_pose,escape_along
from study_native_ryan_carry import native_pose,frame
from build_whole_carry_candidate import local,compose,descendant
from study_chest_panel_weights import chest_panels

def arm_pairs(s,p,side):
    v=s.body.deform(p);ids=[i for i in s.arm if any(n.endswith('_'+side) for n in s.body.dom[s.body.tris[i]])]
    pairs=geo.triangle_crossings(v[s.body.tris[ids]],v[s.body.tris[s.torso]])
    return [(int(ids[i]),int(s.torso[j])) for i,j in pairs if not set(s.auth[s.body.tris[ids[i]]])&set(s.auth[s.body.tris[s.torso[j]]])]

def authored(s,source,action):
    p=native_pose(s,source,action);q=p['DJ_wrist_R'][1]*s.hold['DJ_wrist_R'][1].inv()
    if action=='AimRifle':q=frame(np.array([0.,1,0]),np.array([0.,0,1]))*frame(s.hold['M4_frontsight'][0]-s.hold['M4_rearsight'][0],s.hold['M4_sightup'][0]-s.hold['M4_rearsight'][0]).inv()
    origin=p['DJ_wrist_R'][0]+q.apply(s.hold['M4_body'][0]-s.hold['DJ_wrist_R'][0])
    for n in s.held:
        v=s.hold[n];p[n]=(origin+q.apply(v[0]-s.pivot),q*v[1],v[2])
    s.q=q;s.base=p;return p

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--head-source',action='store_true');parser.add_argument('--head-orientation',action='store_true');parser.add_argument('--seat-stock',action='store_true');args=parser.parse_args()
    source=json.loads((BASE/'NativeRyanCarryStudy/source.json').read_text());out=BASE/('NativeStockSeatStudy' if args.seat_stock else 'NativeHeadOrientationStudy' if args.head_orientation else 'NativeHeadContactStudy' if args.head_source else 'NativeContactArcStudy');out.mkdir(exist_ok=True)
    for action in (['AimRifle'] if args.head_source or args.head_orientation or args.seat_stock else ['AimRifle','HoldRifle-loop']):
        s=SurfaceMount(BASE/'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30',90);s.body,wr,weights=chest_panels(s);base=authored(s,source,action);arcs={};p,m=s.mount(np.zeros(3));before=s.check(p)
        delta=np.zeros(3)
        if args.seat_stock:
            bv=s.body.deform(base);gv=s.gun.deform(base);faces=np.unique(np.r_[s.head,s.chest]);direction=-unit(base['M4_frontsight'][0]-base['M4_rearsight'][0])
            contact=escape_along(bv[s.body.tris[faces]],gv[s.gun.tris],direction,first_contact=True)
            assert np.isfinite(contact) and contact>0,'Source pose already intersects or never reaches shoulder'
            delta=direction*max(0.,contact-.15)
            print('first actual body/gun contact along barrel',contact,'assembly move',delta,flush=True);p,m=s.mount(delta)
        if args.head_source or args.head_orientation:
            source_pose=load_pose(s.run/'source-poses.csv',90)
            import csv
            with (REF.parent/'WholeCarrySource/bind.csv').open() as f:sb={r['bone']:transform({'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}) for r in csv.DictReader(f)}
            for n,donor in [('Neck','neck_01'),('Head','head')]:
                q=source_pose[donor][1]*sb[donor][1].inv()*s.bind[n][1];pos=base[n][0]
                if n=='Head':pos=base['Neck'][0]+(base['Neck'][1]*s.bind['Neck'][1].inv()).apply(s.bind[n][0]-s.bind['Neck'][0])
                base[n]=(pos,q,base[n][2])
            for n in s.names:
                if n not in ['Head','Neck'] and descendant(n,'Head',s.parents):base[n]=compose(local(s.bind[n],s.bind[s.parents[n]]),base[s.parents[n]])
            if args.head_source:
                original=SurfaceMount(s.run,90);optic=s.base['M4_body'][0]+s.q.apply(original.q.inv().apply(original.sight-original.base['M4_body'][0]));delta=base['Camera'][0]-optic;delta-=original.forward*np.dot(delta,original.forward)
                bv=s.body.deform(base);gv=s.gun.deform(base)+delta;faces=np.unique(np.r_[s.head,s.chest]);escape=escape_along(bv[s.body.tris[faces]],gv[s.gun.tris],original.forward);delta+=original.forward*escape
                print('native head source camera registration',delta,escape,flush=True)
            p,m=s.mount(delta)
        for side in ['L','R']:
            if not arm_pairs(s,p,side):arcs[side]=0.;continue
            u,e,w=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];axis=unit(p[w][0]-p[u][0]);center=p[u][0]+axis*np.dot(p[e][0]-p[u][0],axis);radius=p[e][0]-center
            outward=(p['Chest'][1]*s.bind['Chest'][1].inv()).apply(np.array([1 if side=='L' else -1,0.,0.]));direction=np.sign(np.dot(np.cross(axis,radius),outward))
            trial=dict(arcs);trial[side]=30*direction;hi,mm=s.mount(delta,trial)
            if arm_pairs(s,hi,side) or max(mm['wrist_deg'])>40.001:
                print(action,side,'NO_BOUNDED_CLEARANCE',len(arm_pairs(s,hi,side)),mm,flush=True);arcs[side]=0.;continue
            low,high=0.,30.
            for _ in range(16):
                mid=(low+high)/2;trial[side]=mid*direction;candidate,cm=s.mount(delta,trial)
                if arm_pairs(s,candidate,side):low=mid
                else:high=mid
            arcs[side]=(high+.2)*direction;p,m=s.mount(delta,arcs)
        p,m=s.mount(delta,arcs);m.update(s.check(p));m.update({'outward_arc_correction_deg':arcs,'before':before,'chest_panel_authored_vertices_changed':wr['authored_vertices_changed'],'mesh_bind_unchanged':True,'source_action':action,'head_source':args.head_source,'head_orientation_source':args.head_orientation,'seat_stock':args.seat_stock,'delta_cm':delta.tolist(),'runtime_enabled':False,'visual_acceptance':False});s.save(out,action.replace('-loop',''),p,m)
if __name__=='__main__':main()
