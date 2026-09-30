"""Blender applies only the scoped local contour to the isolated derivative."""
from pathlib import Path
import bpy,json
from mathutils import Vector
base=Path(__file__).resolve().parents[1]/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved/BodyProportionStudy';out=base.parent/'HeadContourStudy';out.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(base/'candidate.blend'),use_scripts=False);mesh=next(o for o in bpy.data.objects if o.type=='MESH');rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');before={b.name:[list(row) for row in b.matrix_local] for b in rig.data.bones};data=json.loads((out/'contour.json').read_text());inv=mesh.matrix_world.inverted()
assert len(mesh.data.vertices)==len(data['vertices_cm'])
for vertex,point in zip(mesh.data.vertices,data['vertices_cm']):vertex.co=inv@Vector((point[0]/100,-point[1]/100,point[2]/100))
mesh.data.update();after={b.name:[list(row) for row in b.matrix_local] for b in rig.data.bones};assert before==after
for obj in bpy.context.scene.objects:obj.select_set(False)
mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(out/'candidate.blend'))
bpy.ops.export_scene.fbx(filepath=str(out/'candidate.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
(out/'construction.json').write_text(json.dumps(data['report'],indent=2));print('HEAD_CONTOUR_EXPORTED',out,'bones',len(before))
