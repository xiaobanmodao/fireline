"""Export an isolated weight-only chest-panel correction; no original writes."""
from pathlib import Path
import bpy,json
ROOT=Path(__file__).resolve().parents[1];base=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';data=base/'ReferenceProject/Saved/BodyProportionStudy';out=base/'ReferenceProject/Saved/ChestPanelStudy';out.mkdir(exist_ok=True)
report=json.loads((base/'ChestPanelStudy/report.json').read_text());weights=json.loads((base/'ChestPanelStudy/weights.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(data/'candidate.blend'),use_scripts=False);mesh=next(o for o in bpy.data.objects if o.type=='MESH');rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
positions=[tuple(v.co) for v in mesh.data.vertices];matrices={b.name:[list(r) for r in b.matrix_local] for b in rig.data.bones}
assert len(mesh.data.vertices)==len(weights)
for i in report['changed_ids']:
    for group in mesh.vertex_groups:
        if group.name.startswith(('Shoulder_','UpperArm_')):group.remove([i])
    mesh.vertex_groups['Chest'].add([i],weights[i]['Chest'],'REPLACE')
assert positions==[tuple(v.co) for v in mesh.data.vertices]
assert matrices=={b.name:[list(r) for r in b.matrix_local] for b in rig.data.bones}
for obj in bpy.context.scene.objects:obj.select_set(False)
mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(out/'candidate.blend'))
bpy.ops.export_scene.fbx(filepath=str(out/'candidate.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
print('CHEST_PANEL_EXPORTED',len(report['changed_ids']),'weight vertices; original positions/bind unchanged')
