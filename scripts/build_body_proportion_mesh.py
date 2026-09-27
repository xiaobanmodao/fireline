"""Build a separate Ryan/DJ derivative; never writes selected source assets.

Run with Blender. Uses exported native mesh and measured ALS shoulder/height
ratio. Retains native hand surface and weights, reconnects existing cuff rims.
"""
from pathlib import Path
import bpy,bmesh,json,math,hashlib
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
D=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved/BodyProportionStudy'
# Regenerate Blender-side measurements from the exact exported FBX inputs so
# rebuilding never depends on an untracked preparation script or stale blend.
for label in ['native','source']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(D/(label+'.fbx')))
 metadata=[]
 for obj in bpy.data.objects:
  item={'name':obj.name,'type':obj.type,'scale':list(obj.scale),'dim':list(obj.dimensions)}
  if obj.type=='ARMATURE':item['bones']={b.name:{'head':list(obj.matrix_world@b.head_local),'tail':list(obj.matrix_world@b.tail_local)} for b in obj.data.bones}
  if obj.type=='MESH':item.update(materials=[m.name for m in obj.data.materials],verts=len(obj.data.vertices),faces=len(obj.data.polygons))
  metadata.append(item)
 (D/(label+'-blender.json')).write_text(json.dumps(metadata,indent=2))
 bpy.ops.wm.save_as_mainfile(filepath=str(D/(label+'.blend')))
bpy.ops.wm.open_mainfile(filepath=str(D/'native.blend'),use_scripts=False)
mesh=next(o for o in bpy.data.objects if o.type=='MESH');rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
source=json.loads((D/'source-blender.json').read_text());native=json.loads((D/'native-blender.json').read_text())
sb=next(o['bones'] for o in source if o['type']=='ARMATURE');nb=next(o['bones'] for o in native if o['type']=='ARMATURE')
source_height=next(o['dim'][2] for o in source if o['type']=='MESH');native_height=mesh.dimensions.z
shoulder_scale=(abs(sb['upperarm_l']['head'][0])*native_height/source_height)/abs(nb['UpperArm_L']['head'][0])
assert .76<shoulder_scale<.82
old={b.name:rig.matrix_world@b.head_local for b in rig.data.bones}
shifts={};parents={b.name:b.parent.name if b.parent else None for b in rig.data.bones}
def under(n,p):
 while n:
  if n==p:return True
  n=parents.get(n)
 return False
for n in old:
 shift=Vector()
 for side in ['L','R']:
  if n=='Shoulder_'+side:shift.x=old[n].x*(shoulder_scale-1)
  elif under(n,'UpperArm_'+side):shift.x=old['UpperArm_'+side].x*(shoulder_scale-1)
 shifts[n]=shift
# Body-only sectional shaping; hand geometry is a rigid translation, never a
# nonuniform scale. Skin weights interpolate the neighboring body sections.
mesh_inv=mesh.matrix_world.inverted();group_names={g.index:g.name for g in mesh.vertex_groups}
def smooth_range(value,lo,hi):
 t=max(0,min(1,(value-lo)/(hi-lo)))
 return t*t*(3-2*t)
def lower_sections():
 # Compare silhouettes in the same rest space, not dominant bone groups:
 # Ryan's lateral hip armor is weighted to the thighs rather than Pelvis.
 points=[mesh.matrix_world@v.co for v in mesh.data.vertices]
 result={}
 for name,planes in [('hip',[.98,1.04,1.10]),('thigh',[.75,.85]),('shin',[.25,.40]),('boots',[.02,.04])]:
  # Long low-poly panels need edge/plane intersections: some sections have
  # no authored vertices at all, so a vertex-only band would be misleading.
  selected=[]
  for z in planes:
   for e in mesh.data.edges:
    a,b=[points[i] for i in e.vertices]
    if abs(a.z-b.z)>1e-8 and min(a.z,b.z)<=z<=max(a.z,b.z):selected.append(a.lerp(b,(z-a.z)/(b.z-a.z)))
  assert selected,name
  result[name]={'width_cm':100*(max(p.x for p in selected)-min(p.x for p in selected)),
                'depth_cm':100*(max(p.y for p in selected)-min(p.y for p in selected))}
 return result
lower_before=lower_sections()
for v in mesh.data.vertices:
 p=mesh.matrix_world@v.co;result=Vector();total=0
 for g in v.groups:
  n=group_names[g.group];a=p.copy();center=old.get(n,Vector())
  if n=='Chest':a=Vector((p.x*.84,p.y*.74,p.z))
  elif n=='Torso':a=Vector((p.x*.91,p.y*.88,p.z))
  elif n=='Pelvis' or n.startswith('UpperLeg_'):
   # A small silhouette correction, not a new leg rig. Fade out above the
   # knee so the hinge, armor opening and existing swing remain unchanged.
   hip=smooth_range(p.z,old['LowerLeg_L'].z+.07,old['UpperLeg_L'].z)
   thickness=smooth_range(p.z,old['LowerLeg_L'].z+.06,old['LowerLeg_L'].z+.20)
   a=Vector((p.x*(1-.06*hip),p.y*(1-.08*thickness),p.z))
  elif n.startswith('LowerLeg_'):
   # Retain both knee and ankle rim geometry; reduce only the shin bulge.
   taper=smooth_range(p.z,old['Foot_L'].z+.06,old['Foot_L'].z+.16)*(1-smooth_range(p.z,old['LowerLeg_L'].z-.15,old['LowerLeg_L'].z-.06))
   a=Vector((p.x,center.y+(p.y-center.y)*(1-.04*taper),p.z))
  elif n.startswith('Shoulder_'):a=center+shifts[n]+Vector(((p-center).x*shoulder_scale,(p-center).y*.76,(p-center).z*.88))
  elif n.startswith('UpperArm_'):a=center+shifts[n]+Vector(((p-center).x,(p-center).y*.85,(p-center).z*.85))
  elif n.startswith('LowerArm_'):a=center+shifts[n]+Vector(((p-center).x,(p-center).y*.92,(p-center).z*.92))
  else:a=p+shifts.get(n,Vector())
  result+=a*g.weight;total+=g.weight
 if total:v.co=mesh_inv@(result/total)
lower_after=lower_sections()
# Translate arm bind chains together; keep each bone's axes and length.
bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
edit_original={b.name:(b.matrix.copy(),b.length) for b in rig.data.edit_bones}
for b in rig.data.edit_bones:b.use_connect=False
for b in rig.data.edit_bones:
 matrix,length=edit_original[b.name];matrix.translation+=rig.matrix_world.to_3x3().inverted()@shifts[b.name]
 b.matrix=matrix;b.length=length
bpy.ops.object.mode_set(mode='OBJECT')
# Weld only coincident render splits. The source has three components and four
# open rims: two 6-vertex sleeve rims and two 30-vertex native glove liner rims.
bm=bmesh.new();bm.from_mesh(mesh.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001)
def loops():
 boundary=[v for v in bm.verts if v.is_boundary];seen=set();out=[]
 for v in boundary:
  if v in seen:continue
  todo=[v];seen.add(v);part=[]
  while todo:
   q=todo.pop();part.append(q)
   for e in q.link_edges:
    z=e.other_vert(q)
    if e.is_boundary and z not in seen:seen.add(z);todo.append(z)
  out.append(part)
 return out
rims=loops();assert sorted(map(len,rims))==[6,6,30,30]
added=0
for sign in [-1,1]:
 a,b=sorted([r for r in rims if sum((mesh.matrix_world@v.co).x for v in r)*sign>0],key=len)
 center=sum((mesh.matrix_world@v.co for v in b),Vector())/len(b)
 def theta(v):
  p=mesh.matrix_world@v.co-center;return math.atan2(p.z,p.y)%(2*math.pi)
 def ordered(group):
  result=[group[0]];current=group[0];previous=None
  while len(result)<len(group):
   nxt=next(e.other_vert(current) for e in current.link_edges if e.is_boundary and e.other_vert(current)!=previous and e.other_vert(current)!=result[0])
   result.append(nxt);previous,current=current,nxt
  return result
 a=ordered(a);b=ordered(b)
 def normal(r):
  pts=[mesh.matrix_world@v.co-center for v in r]
  return sum((pts[i].cross(pts[(i+1)%len(r)]) for i in range(len(r))),Vector())
 if normal(a).dot(normal(b))<0:b.reverse()
 first=min(range(len(b)),key=lambda i:(b[i].co-a[0].co).length);b=b[first:]+b[:first]
 # Native sleeve/glove loops contain 6/30 vertices: bridge each topological
 # sleeve edge to exactly five consecutive glove edges, without sorting away
 # the original edge order on a tilted/asymmetric wrist opening.
 for i in range(6):
  for j in range(5):
   k=i*5+j;f=bm.faces.new((a[i],b[k%30],b[(k+1)%30]));f.material_index=0;f.smooth=True;added+=1
  f=bm.faces.new((a[i],b[((i+1)*5)%30],a[(i+1)%6]));f.material_index=0;f.smooth=True;added+=1
assert not loops(),'Unclosed cuff rim'
# Keep Ryan armor faceted and only the native DJ hand surface smooth. Welding
# render splits must not silently turn the low-poly body into smooth shading.
deform=bm.verts.layers.deform.active
for f in bm.faces:
    f.smooth=all(sum(w for g,w in v[deform].items() if group_names[g].startswith('DJ_'))>.5 for v in f.verts)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh.data);bm.free();mesh.data.update()
# No new topology touches the native palm/fingers. Export the isolated rig and
# mesh only, letting the FBX exporter retain their world transforms.
rig.animation_data_clear();rig.data.pose_position='REST'
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.wm.save_as_mainfile(filepath=str(D/'candidate.blend'))
bpy.ops.export_scene.fbx(filepath=str(D/'candidate.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
report={'source_height_m':source_height,'native_height_m':native_height,'height_changed':False,'shoulder_span_old_cm':abs(old['UpperArm_L'].x-old['UpperArm_R'].x)*100,'shoulder_span_new_cm':abs(old['UpperArm_L'].x-old['UpperArm_R'].x)*shoulder_scale*100,'shoulder_ratio_from_source':shoulder_scale,'chest_width_scale':.84,'chest_depth_scale':.74,'lower_body':{'hip_width_scale_max':.94,'hip_thigh_depth_scale_max':.92,'shin_depth_scale_max':.96,'sections_before':lower_before,'sections_after':lower_after,'joint_positions_changed':False,'sole_and_boot_shape_changed':False,'note':'Small silhouette refinement toward reference; not a claim of exact ALS dimensions. Knee/ankle rims preserved; geometry taper blended by original weights.'},'cuff_bridge_triangles':added,'remaining_boundary_loops':0,'original_mesh_vertices':9187,'merged_vertices':len(mesh.data.vertices),'bind_translations_cm':{n:list(d*100) for n,d in shifts.items() if d.length>0},'source_sha256':hashlib.sha256((D/'native.fbx').read_bytes()).hexdigest(),'scope':'isolated derivative; source unchanged; lower silhouette refined; all leg joints, hand shape and recorded motion preserved'}
(D/'construction.json').write_text(json.dumps(report,indent=2));print('BODY_PROPORTION_MESH',json.dumps({k:v for k,v in report.items() if k!='bind_translations_cm'}))
