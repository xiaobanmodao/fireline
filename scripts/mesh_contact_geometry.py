"""Triangle surface crossings, separate from conservative convex-envelope tests."""
import numpy as np

def triangle_crossings(a, b, epsilon=1e-6):
    """Return intersecting triangle index pairs, including touching surfaces.

    Broad phase uses AABBs; narrow phase uses triangle plane normals, edge
    cross products and in-plane edge normals (also handles coplanar triangles).
    This tests surface intersections, not containment of one closed mesh inside
    another. It does not weld, close, or otherwise modify input geometry.
    """
    amin,amax=a.min(1),a.max(1);bmin,bmax=b.min(1),b.max(1)
    pairs=[]
    for i in range(len(a)):
        candidates=np.flatnonzero(((bmax>=amin[i]-epsilon)&(bmin<=amax[i]+epsilon)).all(1))
        pairs.extend((i,int(j)) for j in candidates)
    if not pairs:return np.empty((0,2),int)
    pairs=np.array(pairs);aa=a[pairs[:,0]];bb=b[pairs[:,1]]
    ea=np.roll(aa,-1,axis=1)-aa;eb=np.roll(bb,-1,axis=1)-bb
    na=np.cross(ea[:,0],ea[:,1]);nb=np.cross(eb[:,0],eb[:,1])
    axes=np.concatenate([na[:,None],nb[:,None],np.cross(ea[:,:,None,:],eb[:,None,:,:]).reshape(-1,9,3),np.cross(na[:,None],ea),np.cross(nb[:,None],eb)],axis=1)
    norms=np.linalg.norm(axes,axis=2);active=norms>1e-10
    axes/=np.maximum(norms[:,:,None],1e-20)
    pa=np.einsum('nkj,nij->nki',axes,aa);pb=np.einsum('nkj,nij->nki',axes,bb)
    separated=active&((pa.max(2)<pb.min(2)-epsilon)|(pb.max(2)<pa.min(2)-epsilon))
    return pairs[~separated.any(1)]

def check_examples():
    a=np.array([[[0,0,0],[2,0,0],[0,2,0]]],float)
    crossing=np.array([[[.5,.5,-1],[.5,.5,1],[1.5,.5,0]]])
    coplanar_out=np.array([[[1.5,1.5,0],[3,1.5,0],[1.5,3,0]]])
    coplanar_in=np.array([[[.1,.1,0],[.4,.1,0],[.1,.4,0]]])
    parallel=np.array([[[0,0,.1],[2,0,.1],[0,2,.1]]])
    assert len(triangle_crossings(a,crossing))==1
    assert len(triangle_crossings(a,coplanar_in))==1
    assert len(triangle_crossings(a,coplanar_out))==0
    assert len(triangle_crossings(a,parallel))==0
    assert len(triangle_crossings(crossing,a))==1
    assert len(triangle_crossings(-a,-crossing))==1

if __name__=='__main__':check_examples();print('Triangle crossing examples passed')
