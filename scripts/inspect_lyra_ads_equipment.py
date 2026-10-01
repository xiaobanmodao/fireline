"""Read original Lyra equipment and component attachment; no asset saves."""
import unreal as u
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ADSLayoutStudy'
out.mkdir(exist_ok=True)
def tr(t):
    return {'p':[t.translation.x,t.translation.y,t.translation.z],
            'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
            's':[t.scale3d.x,t.scale3d.y,t.scale3d.z]}
cls=u.load_class(None,'/ShooterCore/Weapons/Rifle/WID_Rifle.WID_Rifle_C')
assert cls,'Original ShooterCore WID_Rifle must load in the official Lyra project'
cdo=u.get_default_object(cls);records=[]
subsystem=u.get_editor_subsystem(u.EditorActorSubsystem)
for info in cdo.get_editor_property('actors_to_spawn'):
    actor_cls=info.get_editor_property('actor_to_spawn')
    actor=subsystem.spawn_actor_from_class(actor_cls,u.Vector(0,0,0),u.Rotator(0,0,0),transient=True)
    assert actor
    parts=[]
    for comp in actor.get_components_by_class(u.SceneComponent):
        asset=comp.get_editor_property('skeletal_mesh_asset') if isinstance(comp,u.SkeletalMeshComponent) else None
        parts.append({'name':comp.get_name(),'class':comp.get_class().get_name(),
                      'mesh':asset.get_path_name() if asset else None,
                      'relative':tr(comp.get_relative_transform()),
                      'world_at_identity_actor':tr(comp.get_world_transform()),
                      'attach_socket':str(comp.get_attach_socket_name()),
                      'parent':comp.get_attach_parent().get_name() if comp.get_attach_parent() else None})
    records.append({'actor_class':actor_cls.get_path_name(),'socket':str(info.get_editor_property('attach_socket')),
                    'equipment_relative':tr(info.get_editor_property('attach_transform')),'components':parts})
    subsystem.destroy_actor(actor)
mesh=u.load_asset('/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny')
assert mesh
sockets=[]
for name in ['weapon_r','hand_r','head','neck_01','neck_02','spine_05']:
    socket=mesh.find_socket(name)
    sockets.append({'name':name,'socket_found':bool(socket),'bone':str(socket.get_editor_property('bone_name')) if socket else None,
                    'offset_location':str(socket.get_editor_property('relative_location')) if socket else None,
                    'offset_rotation':str(socket.get_editor_property('relative_rotation')) if socket else None})
(out/'lyra-equipment.json').write_text(json.dumps({'equipment_class':cls.get_path_name(),'actors':records,'source_socket_probes':sockets,'assets_saved':False,'original_runtime_graph_evaluated':False},indent=2))
u.log('LYRA_ADS_EQUIPMENT_COMPLETE actors='+str(len(records)))
