"""Read actual source mesh binds in the isolated reference project. No asset saves."""
import unreal as u
from pathlib import Path
out=Path(u.Paths.project_dir()).resolve().parent/'ADSLayoutStudy'
out.mkdir(exist_ok=True)
for name,path in [('lyra','/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny'),
                  ('lyra-rifle','/Game/Weapons/Rifle/Mesh/SK_Rifle')]:
    mesh=u.load_asset(path)
    assert mesh,path
    u.MotionResearchLibrary.finish_reference_compilation()
    assert u.MotionResearchLibrary.export_mesh_reference(mesh,str(out/(name+'.bind.csv')))
u.log('ADS_REFERENCE_BIND_COMPLETE actual_mesh_reference=1 assets_saved=0')
