"""Read actual UE render buffers for the isolated proportion/carry study."""
from pathlib import Path
import csv
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull

def rows(p):
 with p.open(encoding='utf-8-sig') as f:return list(csv.DictReader(f))
class Surface:
 def __init__(self,base):
  base=Path(base);self.v=np.array([[float(r[k]) for k in ['x','y','z']] for r in rows(Path(str(base)+'.vertices.csv'))])
  self.tris=np.array([[int(r[k]) for k in ['a','b','c']] for r in rows(Path(str(base)+'.triangles.csv'))])
  bind={r['bone']:r for r in rows(Path(str(base)+'.bind.csv'))};groups={};self.dom=np.empty(len(self.v),object);best=np.zeros(len(self.v))
  for r in rows(Path(str(base)+'.weights.csv')):groups.setdefault(r['bone'],[]).append((int(r['vertex']),int(r['weight'])/65535))
  self.skin={}
  for n,data in groups.items():
   ids=np.array([i for i,w in data]);w=np.array([w for i,w in data]);b=bind[n];p=np.array([float(b[k]) for k in ['x','y','z']]);q=R.from_quat([float(b[k]) for k in ['qx','qy','qz','qw']]);s=np.array([float(b[k]) for k in ['sx','sy','sz']]);local=q.inv().apply(self.v[ids]-p)/s
   self.skin[n]=(ids,w,local);take=w>best[ids];self.dom[ids[take]]=n;best[ids[take]]=w[take]
 def deform(self,P,ids=None):
  out=np.zeros_like(self.v)
  for n,(inds,w,local) in self.skin.items():
   t=P[n];p,q,s=(t if isinstance(t,tuple) else (np.array(t['p']),R.from_quat(t['q']),np.array(t['s'])))
   out[inds]+=(q.apply(local*s)+p)*w[:,None]
  return out if ids is None else out[ids]
 def subset(self,ids):
  other=object.__new__(Surface);other.v=self.v[ids];other.dom=self.dom[ids];other.tris=None;other.skin={};lookup={v:i for i,v in enumerate(ids)}
  for n,(inds,w,local) in self.skin.items():
   keep=np.array([v in lookup for v in inds]);ii=np.array([lookup[v] for v in inds[keep]])
   if len(ii):other.skin[n]=(ii,w[keep],local[keep])
  return other
 def hull_samples(self,predicate):
  ids=[]
  for n in np.unique(self.dom):
   if not predicate(n):continue
   group=np.flatnonzero(self.dom==n)
   # All skin seam/extreme points retained; coordinate duplicates removed.
   _,unique=np.unique(self.v[group].round(5),axis=0,return_index=True);group=group[unique]
   if len(group)>4:
    try:group=group[ConvexHull(self.v[group],qhull_options='QJ').vertices]
    except Exception:pass
   ids.extend(group)
  return self.subset(np.array(ids))

def chest_hull(surface,P):
 return ConvexHull(surface.deform(P)).equations

def signed(points,hull):
 assert np.isfinite(points).all() and np.isfinite(hull).all()
 # Explicit three-term dot avoids accelerated BLAS floating-status warnings
 # on this Apple/NumPy build and is trivial for three-component geometry.
 values=np.einsum('ij,kj->ik',points,hull[:,:3],optimize=False)+hull[:,3]
 assert np.isfinite(values).all()
 return values.max(axis=1)
