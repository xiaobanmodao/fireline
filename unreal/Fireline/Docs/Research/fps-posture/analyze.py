"""Recompute positional posture measurements from preserved original-source samples.
Usage: python3 analyze.py /path/to/old/unreal/Fireline/Docs/Research
No animation/mesh is modified. Output contains aggregates, not animation keys.
"""
import gzip, hashlib, json, math, statistics, sys
from pathlib import Path

def sub(a,b): return [x-y for x,y in zip(a,b)]
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def norm(a): return math.sqrt(dot(a,a))
def unit(a):
 n=norm(a)
 if n<1e-9: raise ValueError('Degenerate vector')
 return [x/n for x in a]
def angle(a,b): return math.degrees(math.acos(max(-1,min(1,dot(unit(a),unit(b))))))
def wrap(a): return (a+180)%360-180
def stats(a):
 s=sorted(a)
 return {k:round(v,4) for k,v in {'min':s[0],'median':statistics.median(s),'max':s[-1],'range':s[-1]-s[0]}.items()}
def points(sample,kind):
 b=sample['bones']
 # Both exports have positive X to anatomical left in neutral stance.
 # Canonical output: X=asset-forward, Y=left, Z=up. Positions, not Euler rotations.
 if kind=='MCO':
  names={'hip':'Hips','neck':'Neck','head':'Head', 'sl':'LeftArm','sr':'RightArm','el':'LeftForeArm','er':'RightForeArm','wl':'LeftHand','wr':'RightHand','hl':'LeftUpLeg','hr':'RightUpLeg','kl':'LeftLeg','kr':'RightLeg','fl':'LeftFoot','fr':'RightFoot'}
  convert=lambda p:[p[2],p[0],p[1]]
 else:
  names={'hip':'pelvis','neck':'neck_01','head':'head','sl':'upperarm_l','sr':'upperarm_r','el':'lowerarm_l','er':'lowerarm_r','wl':'hand_l','wr':'hand_r','hl':'thigh_l','hr':'thigh_r','kl':'calf_l','kr':'calf_r','fl':'foot_l','fr':'foot_r'}
  convert=lambda p:[p[1],p[0],p[2]]
 return {k:convert(b[v]['p']) for k,v in names.items()}
def measure(p):
 torso=sub(p['neck'],p['hip']); shoulder=sub(p['sl'],p['sr']); hips=sub(p['hl'],p['hr'])
 width=norm(shoulder); lateral=unit([shoulder[0],shoulder[1],0])
 heading=lambda v:math.degrees(math.atan2(-v[0],v[1]))
 out={'torso_total_tilt_deg':angle(torso,[0,0,1]),'asset_forward_lean_deg':math.degrees(math.atan2(torso[0],torso[2])),
 'shoulder_minus_hip_heading_deg':wrap(heading(shoulder)-heading(hips)),
 'hand_separation_cm':norm(sub(p['wl'],p['wr'])), 'shoulder_width_cm':width}
 for s,sign in [('l',1),('r',-1)]:
  out[s+'_elbow_interior_deg']=angle(sub(p['s'+s],p['e'+s]),sub(p['w'+s],p['e'+s]))
  out[s+'_knee_flexion_deg']=180-angle(sub(p['h'+s],p['k'+s]),sub(p['f'+s],p['k'+s]))
  out[s+'_elbow_outside_shoulder_cm']=sign*dot(sub(p['e'+s],p['s'+s]),lateral)
  out[s+'_upperarm_cm']=norm(sub(p['s'+s],p['e'+s]))
  out[s+'_forearm_cm']=norm(sub(p['w'+s],p['e'+s]))
  out[s+'_reach_fraction']=norm(sub(p['w'+s],p['s'+s]))/(out[s+'_upperarm_cm']+out[s+'_forearm_cm'])
 return out

def main():
 root=Path(sys.argv[1]);dest=Path(__file__).parent; rows=[];inputs=[];poses={}
 for kind,rel in [('MCO','downloaded-locomotion/mco-source-samples.json.gz'),('Lyra','strafe-source-review/lyra/samples.json.gz')]:
  path=root/rel;data=json.load(gzip.open(path,'rt'));inputs.append({'file':rel,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
  for c in data:
   name=c.get('name') or Path(c['file']).stem
   if kind=='MCO' and name in ['W2_Jog_Aim_F_Loop','W2_Walk_Aim_F_Loop']:continue # RM duplicates of IPC
   pts=[points(s,kind) for s in c['samples']];ms=[measure(p) for p in pts]
   row={'source':kind,'name':name,'duration_s':c['duration'],'samples':len(ms),'metrics':{k:stats([m[k] for m in ms]) for k in ms[0]}}
   if 'To_Relaxed' in name:row['endpoint_metrics']={'first':ms[0],'last':ms[-1]}
   rows.append(row);poses[name]=pts[0]
 output={'date':'2026-09-27','inputs':inputs,'clip_count':len(rows),'sample_count':sum(r['samples'] for r in rows),'notes':['Original-source samples; no runtime/retarget measurements.','Centimetres. Elbow interior angle is 180 degrees at full extension. Knee flexion is zero at extension.','Total torso tilt is pelvis-to-neck versus vertical; it includes sideways tilt. Forward lean uses source asset axes, not camera axes.','Elbow outside shoulder is a projection, not mesh collision or a human joint limit.','No mass distribution; pelvis is not a measured centre of mass.','Sample range is not an anatomical limit or visual acceptance.'], 'clips':rows}
 (dest/'metrics.json').write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n')
 lines=['| 原片 | 躯干总倾角中位数 | 肩线相对髋线偏航中位数 | 左/右肘内角中位数 | 双腕距离变化范围 |','|---|---:|---:|---:|---:|']
 for r in rows:
  m=r['metrics'];lines.append(f"| {r['name']} | {m['torso_total_tilt_deg']['median']:.1f}° | {m['shoulder_minus_hip_heading_deg']['median']:.1f}° | {m['l_elbow_interior_deg']['median']:.1f}° / {m['r_elbow_interior_deg']['median']:.1f}° | {m['hand_separation_cm']['range']:.2f} cm |")
 (dest/'measurements.md').write_text('\n'.join(lines)+'\n')
 # Orthographic skeleton illustration, no mesh, weapon or invented bone poses.
 selected=['W2_Stand_Aim_Idle_v2','W2_Stand_Relaxed_Idle_v2','W2_Walk_Aim_F_Loop_IPC','W2_Jog_Aim_F_Loop_IPC','MM_Rifle_Walk_Left','MM_Rifle_Walk_Right']
 svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1320" height="850" viewBox="0 0 1320 850"><rect width="1320" height="850" fill="#101923"/><g font-family="sans-serif" fill="#e5edf7"><text x="22" y="30" font-size="20">Original source poses — first sample, orthographic front / side</text><text x="22" y="54" font-size="13">Cyan = left chain; orange = right chain. Skeleton only; not mesh collision or runtime acceptance.</text>']
 for i,name in enumerate(selected):
  p=poses[name];cx=(i%3)*440;cy=(i//3)*380+75
  svg.append(f'<text x="{cx+16}" y="{cy+15}" font-size="13">{name}</text>')
  for view,(axis,ox) in enumerate([(1,125),(0,295)]):
   def xy(k):return (cx+ox+(p[k][axis]-p['hip'][axis])*(-1 if view==0 else 1)*1.45,cy+300-(p[k][2]-p['hip'][2]+90)*1.45)
   edges=[('hip','neck','#bec8d4'),('neck','head','#bec8d4')]
   for s,col in [('l','#57d9ee'),('r','#ffa356')]:edges += [('neck','s'+s,col),('s'+s,'e'+s,col),('e'+s,'w'+s,col),('hip','h'+s,col),('h'+s,'k'+s,col),('k'+s,'f'+s,col)]
   for a,b,col in edges:
    x,y=xy(a);u,v=xy(b);svg.append(f'<path d="M{x:.2f},{y:.2f} L{u:.2f},{v:.2f}" stroke="{col}" stroke-width="3" fill="none"/>')
   for k in p:
    x,y=xy(k);svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3" fill="#f0f4f9"/>')
   svg.append(f'<text x="{cx+ox-20}" y="{cy+345}" font-size="12">{"front" if view==0 else "side"}</text>')
 svg.append('</g></svg>');(dest/'source-poses.svg').write_text(''.join(svg))
 print('clips',len(rows),'samples',output['sample_count']);print('\n'.join(lines))
if __name__=='__main__':main()
