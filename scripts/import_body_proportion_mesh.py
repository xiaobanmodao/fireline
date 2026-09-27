"""Import only into the isolated host, with a separate skeleton; export its bind."""
import unreal as u,json
from pathlib import Path
saved=Path(u.Paths.project_saved_dir()).resolve();d=saved/'BodyProportionStudy'
assert 'ReferenceProject' in str(saved)
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
opts=u.FbxImportUI()
for k,v in {'automated_import_should_detect_type':False,'import_mesh':True,'import_as_skeletal':True,'mesh_type_to_import':u.FBXImportType.FBXIT_SKELETAL_MESH,'import_animations':False,'import_materials':False,'import_textures':False,'create_physics_asset':False}.items():opts.set_editor_property(k,v)
for k,v in {'update_skeleton_reference_pose':True,'convert_scene_unit':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():opts.skeletal_mesh_import_data.set_editor_property(k,v)
task=u.AssetImportTask()
for k,v in {'filename':str(d/'candidate.fbx'),'destination_path':'/Game/BodyProportionStudy','destination_name':'SK_RyanProportion','automated':True,'replace_existing':True,'save':True,'options':opts}.items():task.set_editor_property(k,v)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);assert task.imported_object_paths
mesh=u.load_asset('/Game/BodyProportionStudy/SK_RyanProportion');assert mesh
slots=list(mesh.materials)
for slot in slots:
 name=str(slot.material_slot_name)
 if name.startswith('M_Hand'):path='/Game/Fireline/Hands/M_Hand_Glove_Blue'
 elif name.startswith('M_Under'):path='/Game/Fireline/Materials/M_Under'
 elif name.startswith('M_Visor'):path='/Game/Fireline/Materials/M_Visor'
 else:path='/Game/Fireline/Materials/M_Player'
 slot.material_interface=u.load_asset(path);assert slot.material_interface,path
mesh.set_editor_property('materials',slots)
assert mesh.skeleton and mesh.skeleton.get_path_name().startswith('/Game/BodyProportionStudy/'), 'Derivative needs its own persistent skeleton'
assert u.EditorAssetLibrary.save_loaded_asset(mesh.skeleton,False)
u.EditorAssetLibrary.save_loaded_asset(mesh,False)
u.MotionResearchLibrary.finish_reference_compilation()
comp=u.SkeletalMeshComponent();comp.set_skeletal_mesh(mesh)
ref=u.AnimPoseExtensions.get_reference_pose(mesh.skeleton)
u.MotionResearchLibrary.export_mesh_reference(mesh,str(d/'candidate-bind.csv'))
assert u.MotionResearchLibrary.export_mesh_geometry(mesh,str(d/'candidate'))
assert u.MotionResearchLibrary.export_mesh_geometry(u.load_asset('/Game/Fireline/Hands/SK_M4Action'),str(d/'gun'))
u.MotionResearchLibrary.export_mesh_reference(u.load_asset('/Game/Fireline/Hands/SK_RyanAction'),str(d/'native-bind.csv'))
names=list(map(str,u.AnimPoseExtensions.get_bone_names(ref)))
def tr(t):return {'p':list(t.translation.to_tuple()),'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'s':list(t.scale3d.to_tuple())}
old=json.loads((saved/'WholeCarrySource/target.json').read_text())
parents={n:str(comp.get_parent_bone(n)) for n in names}
assert set(names)==set(old['names']),(set(names)-set(old['names']),set(old['names'])-set(names))
result={'mesh':mesh.get_path_name(),'hold_asset':old['hold_asset'],'names':names,'parents':{n:p if p in names else None for n,p in parents.items()},'reference':{n:tr(u.AnimPoseExtensions.get_bone_pose(ref,n,u.AnimPoseSpaces.WORLD)) for n in names},'hold':old['hold']}
import csv
with (d/'candidate-bind.csv').open(encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
result['reference']={r['bone']:{'p':[float(r[k]) for k in ['x','y','z']],'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]} for r in rows}
(d/'target.json').write_text(json.dumps(result));u.log('PROPORTION_MESH_IMPORTED bones='+str(len(names)))
