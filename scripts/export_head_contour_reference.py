"""Blender background export of local reference geometry; no original writes."""
from pathlib import Path
import bpy,json
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved'
OUT=BASE/'HeadContourStudy';OUT.mkdir(exist_ok=True)
result={}
for label in ['source','candidate']:
    if label=='source':
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(BASE/'BodyProportionStudy/source.fbx'))
    else:bpy.ops.wm.open_mainfile(filepath=str(BASE/'BodyProportionStudy/candidate.blend'),use_scripts=False)
    mesh=next(o for o in bpy.data.objects if o.type=='MESH')
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    def ue(p):return [100*p.x,-100*p.y,100*p.z]
    mesh.data.calc_loop_triangles()
    result[label]={'vertices':[ue(mesh.matrix_world@v.co) for v in mesh.data.vertices],
        'edges':[list(e.vertices) for e in mesh.data.edges],
        'triangles':[list(t.vertices) for t in mesh.data.loop_triangles],
        'face_materials':[t.material_index for t in mesh.data.loop_triangles],
        'materials':[m.name for m in mesh.data.materials],
        'weights':[{mesh.vertex_groups[g.group].name:g.weight for g in v.groups} for v in mesh.data.vertices],
        'bones':{b.name:ue(rig.matrix_world@b.head_local) for b in rig.data.bones},
        'height_cm':100*mesh.dimensions.z}
(OUT/'mesh-reference.json').write_text(json.dumps(result))
