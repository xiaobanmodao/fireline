"""UE Python: import a new separate derivative, never shared original assets."""
import unreal as u,json
from pathlib import Path
saved=Path(u.Paths.project_saved_dir()).resolve();assert saved.parent.name=='ReferenceProject';d=saved/'HeadContourStudy';d.mkdir(exist_ok=True)
path='/Game/HeadContourStudy/SK_RyanHeadContourRounded';assert not u.EditorAssetLibrary.does_asset_exist(path),'Candidate exists: preserve/backup it before a deliberate revision'
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');opts=u.FbxImportUI()
for k,v in {'automated_import_should_detect_type':False,'import_mesh':True,'import_as_skeletal':True,'mesh_type_to_import':u.FBXImportType.FBXIT_SKELETAL_MESH,'import_animations':False,'import_materials':False,'import_textures':False,'create_physics_asset':False}.items():opts.set_editor_property(k,v)
for k,v in {'update_skeleton_reference_pose':True,'convert_scene_unit':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():opts.skeletal_mesh_import_data.set_editor_property(k,v)
task=u.AssetImportTask()
for k,v in {'filename':str(d/'candidate.fbx'),'destination_path':'/Game/HeadContourStudy','destination_name':'SK_RyanHeadContourRounded','automated':True,'replace_existing':False,'save':True,'options':opts}.items():task.set_editor_property(k,v)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);assert task.imported_object_paths;mesh=u.load_asset(path);old=u.load_asset('/Game/BodyProportionStudy/SK_RyanProportion');assert mesh and old
mesh.set_editor_property('materials',list(old.materials));assert mesh.skeleton and mesh.skeleton.get_path_name().startswith('/Game/HeadContourStudy/')
assert u.EditorAssetLibrary.save_loaded_asset(mesh.skeleton,False);assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
u.MotionResearchLibrary.finish_reference_compilation();assert u.MotionResearchLibrary.export_mesh_reference(mesh,str(d/'candidate-bind.csv'));assert u.MotionResearchLibrary.export_mesh_geometry(mesh,str(d/'candidate'));u.log('HEAD_CONTOUR_NATIVE_IMPORTED '+mesh.get_path_name())
