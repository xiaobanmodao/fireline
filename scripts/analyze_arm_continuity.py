"""Compare final and intermediate native traces; not a mesh collision/art test."""
import argparse,csv,json,math
from collections import defaultdict
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('folders',nargs='+',type=Path);p.add_argument('--out',type=Path);a=p.parse_args()
def read(folder):
 d=defaultdict(dict)
 weapons={(int(r['group']),float(r['time'])):int(r['weapon']) for r in csv.DictReader((folder/'contact.csv').open())}
 for r in csv.DictReader((folder/'poses.csv').open()):
  if int(r['stage']) in (2,3,5):d[int(r['group']),float(r['time']),int(r['stage'])][r['bone']]=tuple(float(r[k]) for k in ('x','y','z'))
 out={}
 for stage in (2,5,3):
  previous={};events=[];lens=defaultdict(list)
  for (g,t,s),b in sorted(d.items()):
   if s!=stage:continue
   for side in ('L','R'):
    S,E,W=[b[n+'_'+side] for n in ('UpperArm','LowerArm','DJ_wrist')]
    lens['upper_'+side].append(math.dist(S,E));lens['lower_'+side].append(math.dist(E,W))
   if g in previous:
    pt,pb=previous[g]
    if 0<t-pt<.025 and weapons.get((g,t))==weapons.get((g,pt)):
     for n in ('LowerArm_L','LowerArm_R','DJ_wrist_L','DJ_wrist_R'):
      events.append((math.dist(b[n],pb[n]),g,t,n))
   previous[g]=(t,b)
  subsets={'ground':lambda e:e[1]<18,'mp7_ground':lambda e:18<=e[1]<20,'jump':lambda e:20<=e[1]<24,'operations':lambda e:e[1]>=24}
  out[str(stage)]={'largest_steps_cm':sorted(events,reverse=True)[:12],'subset_largest_steps_cm':{k:sorted(filter(test,events),reverse=True)[:4] for k,test in subsets.items()},'bone_lengths_cm':{k:[min(v),max(v)] for k,v in lens.items()}}
 arms=list(csv.DictReader((folder/'arms.csv').open()));out['intermediate_body_metrics']={k:max(float(r[k]) for r in arms) for k in ('contact_error_cm','elbow_penetration_cm')}
 out['note']='Body metrics precede ContactCarry; stage 3 is final. Group resets and mesh/weapon swaps excluded from continuous steps. Source step is stage 2 adapted action blend, not original donor.'
 return out
result={f.name:read(f) for f in a.folders};s=json.dumps(result,indent=2);print(s)
if a.out:a.out.write_text(s+'\n')
