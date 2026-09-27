"""Export selected native and ALS reference meshes for an isolated proportion revision."""
import unreal as u
from pathlib import Path
out=Path(u.Paths.project_saved_dir()).resolve()/'BodyProportionStudy';out.mkdir(exist_ok=True)
for label,path in [('native','/Game/Fireline/Hands/SK_RyanAction'),('source','/ALS/ALS/Character/SKM_Als')]:
 mesh=u.load_asset(path);assert mesh
 u.MotionResearchLibrary.finish_reference_compilation()
 task=u.AssetExportTask();task.object=mesh;task.filename=str(out/(label+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=u.SkeletalMeshExporterFBX()
 options=u.FbxExportOption();options.ascii=False;options.level_of_detail=False;task.options=options
 assert u.Exporter.run_asset_export_task(task),label
 u.log('PROPORTION_REFERENCE_EXPORTED '+label)
