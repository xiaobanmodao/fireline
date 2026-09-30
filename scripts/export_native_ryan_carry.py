"""Export original Ryan authored actions, never modify the provider file."""
import bpy,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=ROOT.parent/'火力对决/assets/source/ryan-character/original/Character.blend'
out=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/NativeRyanCarryStudy';out.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False)
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE' and o.data.bones.get('Chest'))
tr=lambda m:{'p':[float(x)*100 for x in m.to_translation()],'q':[float(x) for x in m.to_quaternion()][1:]+[float(m.to_quaternion()[0])],'s':[float(x) for x in m.to_scale()]}
def convert(m):
    from mathutils import Matrix
    a=Matrix(((1,0,0,0),(0,-1,0,0),(0,0,1,0),(0,0,0,1)));return tr(a@m@a)
result={'source_file':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'parents':{b.name:b.parent.name if b.parent else None for b in rig.data.bones},'bind':{b.name:convert(rig.matrix_world@b.matrix_local) for b in rig.data.bones},'actions':{}}
body=bpy.data.objects.get('Body');assert body
rig.animation_data_create()
for name in ['HoldRifle-loop','AimRifle','CombatIdle-loop']:
    action=bpy.data.actions.get(name);assert action,name;rig.animation_data.action=action
    if hasattr(rig.animation_data,'action_slot') and action.slots:rig.animation_data.action_slot=action.slots[0]
    lo,hi=map(int,action.frame_range);frames=[]
    for f in range(lo,hi+1):
        bpy.context.scene.frame_set(f);dg=bpy.context.evaluated_depsgraph_get();r=rig.evaluated_get(dg)
        frames.append({'frame':f,'bones':{b.name:convert(r.matrix_world@b.matrix) for b in r.pose.bones}})
    result['actions'][name]=frames;print('NATIVE_RYAN_ACTION',name,lo,hi,flush=True)
    bpy.context.scene.frame_set(lo);dg=bpy.context.evaluated_depsgraph_get();evaluated=body.evaluated_get(dg);mesh=evaluated.to_mesh();mesh.calc_loop_triangles();owner={g.index:g.name.replace('.L','_L').replace('.R','_R') for g in body.vertex_groups}
    geometry={'vertices':[[100*p.x,-100*p.y,100*p.z] for v in mesh.vertices for p in [body.matrix_world@v.co]],'triangles':[list(t.vertices) for t in mesh.loop_triangles],'owners':[owner[max(v.groups,key=lambda g:g.weight).group] if v.groups else 'None' for v in mesh.vertices]}
    (out/(name.replace('-loop','')+'-original-geometry.json')).write_text(json.dumps(geometry));evaluated.to_mesh_clear()
(out/'source.json').write_text(json.dumps(result));print('NATIVE_RYAN_EXPORTED',out,'bones',len(result['bind']),flush=True)
