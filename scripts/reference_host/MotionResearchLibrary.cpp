#include "MotionResearchLibrary.h"
#include "AssetCompilingManager.h"
void UMotionResearchLibrary::FinishReferenceCompilation()
{
    FAssetCompilingManager::Get().FinishAllCompilation();
}

#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
bool UMotionResearchLibrary::ExportMeshReference(USkeletalMesh* Mesh,const FString& File)
{
 if(!Mesh)return false;
 const auto& Ref=Mesh->GetRefSkeleton();TArray<FTransform> CS;
 FString Text=TEXT("bone,parent,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
 for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);const auto T=P>=0?Ref.GetRefBonePose()[I]*CS[P]:Ref.GetRefBonePose()[I];CS.Add(T);
  const auto V=T.GetLocation(),S=T.GetScale3D();const auto Q=T.GetRotation();
  Text+=FString::Printf(TEXT("%s,%s,%.12f,%.12f,%.12f,%.12f,%.12f,%.12f,%.12f,%.12f,%.12f,%.12f\n"),*Ref.GetBoneName(I).ToString(),P>=0?*Ref.GetBoneName(P).ToString():TEXT("None"),V.X,V.Y,V.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
 }
 return FFileHelper::SaveStringToFile(Text,*File);
}

#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
bool UMotionResearchLibrary::ExportMeshGeometry(USkeletalMesh* Mesh,const FString& Base)
{
 if(!Mesh||!Mesh->GetResourceForRendering()||Mesh->GetResourceForRendering()->LODRenderData.IsEmpty())return false;
 const auto& LOD=Mesh->GetResourceForRendering()->LODRenderData[0];
 FString Verts=TEXT("vertex,x,y,z\n"),Weights=TEXT("vertex,bone,weight\n"),Tris=TEXT("face,a,b,c,material\n");
 for(uint32 V=0;V<LOD.GetNumVertices();++V){const auto P=LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V);Verts+=FString::Printf(TEXT("%u,%.9f,%.9f,%.9f\n"),V,P.X,P.Y,P.Z);}
 const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
 for(const auto& Section:LOD.RenderSections){
  for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
   for(uint32 J=0;J<LOD.SkinWeightVertexBuffer.GetMaxBoneInfluences();++J){const auto W=LOD.SkinWeightVertexBuffer.GetBoneWeight(V,J);if(W){const auto B=Section.BoneMap[LOD.SkinWeightVertexBuffer.GetBoneIndex(V,J)];Weights+=FString::Printf(TEXT("%u,%s,%u\n"),V,*Mesh->GetRefSkeleton().GetBoneName(B).ToString(),W);}}
  for(uint32 F=0;F<Section.NumTriangles;++F){const uint32 I=Section.BaseIndex+F*3;Tris+=FString::Printf(TEXT("%u,%u,%u,%u,%u\n"),I/3,Indices->Get(I),Indices->Get(I+1),Indices->Get(I+2),Section.MaterialIndex);}
 }
 return ExportMeshReference(Mesh,Base+TEXT(".bind.csv"))&&FFileHelper::SaveStringToFile(Verts,*(Base+TEXT(".vertices.csv")))&&FFileHelper::SaveStringToFile(Weights,*(Base+TEXT(".weights.csv")))&&FFileHelper::SaveStringToFile(Tris,*(Base+TEXT(".triangles.csv")));
}
