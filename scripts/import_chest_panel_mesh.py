"""Import the weight-only mesh as a new private candidate and export native data."""
import unreal as u,json
from pathlib import Path
saved=Path(u.Paths.project_saved_dir()).resolve();d=saved/'ChestPanelStudy';assert 'ReferenceProject' in str(saved)
path='/Game/ChestPanelStudy/SK_RyanChestPanels';assert not u.EditorAssetLibrary.does_asset_exist(path),'Never overwrite a candidate silently'
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
opts=u.FbxImportUI()
for k,v in {'automated_import_should_detect_type':False,'import_mesh':True,'import_as_skeletal':True,'mesh_type_to_import':u.FBXImportType.FBXIT_SKELETAL_MESH,'import_animations':False,'import_materials':False,'import_textures':False,'create_physics_asset':False}.items():opts.set_editor_property(k,v)
for k,v in {'update_skeleton_reference_pose':True,'convert_scene_unit':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():opts.skeletal_mesh_import_data.set_editor_property(k,v)
task=u.AssetImportTask()
for k,v in {'filename':str(d/'candidate.fbx'),'destination_path':'/Game/ChestPanelStudy','destination_name':'SK_RyanChestPanels','automated':True,'replace_existing':False,'save':True,'options':opts}.items():task.set_editor_property(k,v)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=u.load_asset(path);assert mesh
slots=list(mesh.materials)
for slot in slots:
    name=str(slot.material_slot_name)
    p='/Game/Fireline/Hands/M_Hand_Glove_Blue' if name.startswith('M_Hand') else '/Game/Fireline/Materials/M_Under' if name.startswith('M_Under') else '/Game/Fireline/Materials/M_Visor' if name.startswith('M_Visor') else '/Game/Fireline/Materials/M_Player'
    slot.material_interface=u.load_asset(p);assert slot.material_interface
mesh.set_editor_property('materials',slots);assert mesh.skeleton.get_path_name().startswith('/Game/ChestPanelStudy/')
u.EditorAssetLibrary.save_loaded_asset(mesh.skeleton,False);u.EditorAssetLibrary.save_loaded_asset(mesh,False);u.MotionResearchLibrary.finish_reference_compilation()
assert u.MotionResearchLibrary.export_mesh_geometry(mesh,str(d/'candidate'))
assert u.MotionResearchLibrary.export_mesh_reference(mesh,str(d/'candidate-bind.csv'))
u.log('CHEST_PANEL_IMPORTED native geometry and actual mesh bind exported')
