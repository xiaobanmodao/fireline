#include "RangeViewMesh.h"
#include "RangeGame.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

void URangeViewMesh::SetSkeletalMesh(USkeletalMesh* NewMesh,bool bReinitPose)
{
 const bool Changed=NewMesh!=GetSkeletalMeshAsset();
 if(Changed)BoneEnvelopes.Reset();
 Super::SetSkeletalMesh(NewMesh,bReinitPose);
 if(Changed&&HasBegunPlay()){BuildBoneEnvelopes();UpdateBounds();}
}
void URangeViewMesh::BeginPlay()
{
 Super::BeginPlay();
 BuildBoneEnvelopes();
 UpdateBounds();
}
void URangeViewMesh::UpdateFollowerComponent()
{
 Super::UpdateFollowerComponent();
 // Leader-pose weapons move through bone animation even when their component
 // transform stays still. UE's base follower update only marks dynamic data;
 // our pose-dependent culling envelope must advance with the same pose.
 InvalidateCachedBounds();
 UpdateBounds();
 MarkRenderTransformDirty();
}
void URangeViewMesh::BuildBoneEnvelopes()
{
 BoneEnvelopes.Reset();
 const auto* Mesh=GetSkeletalMeshAsset();
 const auto* Render=Mesh?Mesh->GetResourceForRendering():nullptr;
 if(!Render||Render->LODRenderData.IsEmpty())return;
 const auto& Ref=Mesh->GetRefSkeleton();
 TArray<FTransform> Bind;
 TArray<FBox> Boxes;Boxes.Init(FBox(ForceInit),Ref.GetNum());
 for(int I=0;I<Ref.GetNum();++I)
 {
  const int Parent=Ref.GetParentIndex(I);
  Bind.Add(Parent<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[Parent]);
 }
 // One startup scan, including every LOD. Per-frame work is only a few bone boxes.
 for(const auto& LOD:Render->LODRenderData)
 {
  const auto& Positions=LOD.StaticVertexBuffers.PositionVertexBuffer;
  const auto& Weights=LOD.SkinWeightVertexBuffer;
  if(!Positions.GetVertexData()||!Weights.GetDataVertexBuffer()->GetWeightData())
  {
   UE_LOG(LogTemp,Error,TEXT("FIRELINE_VIEW_BOUNDS missing CPU vertex data for %s; run prepare_view_bounds.py"),*Mesh->GetName());
   return;
  }
  for(const auto& Section:LOD.RenderSections)
   for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
    for(uint32 I=0;I<Weights.GetMaxBoneInfluences();++I)
    {
     if(!Weights.GetBoneWeight(V,I))continue;
     const int Bone=Section.BoneMap[Weights.GetBoneIndex(V,I)];
     Boxes[Bone]+=Bind[Bone].InverseTransformPosition(FVector(Positions.VertexPosition(V)));
    }
 }
 for(int I=0;I<Boxes.Num();++I)if(Boxes[I].IsValid)BoneEnvelopes.Add({I,Boxes[I]});
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_VIEW_BOUNDS %s bones=%d"),*GetName(),BoneEnvelopes.Num());
}
FBoxSphereBounds URangeViewMesh::CalcBounds(const FTransform& LocalToWorld) const
{
 if(BoneEnvelopes.IsEmpty()||!AreBoneTransformsValid())return Super::CalcBounds(LocalToWorld);
 FBox Box(ForceInit);
 // Every positive influence contributes an envelope. Linear skinning is a
 // convex combination of those transformed points, so the union also contains
 // blended elbow/hand vertices, not just rigid weapon parts.
 for(const auto& Envelope:BoneEnvelopes)
  Box+=Envelope.LocalBox.TransformBy(GetBoneTransform(Envelope.Bone,LocalToWorld).ToMatrixWithScale());
 // Small numerical margin, not a scene-sized bound or an occlusion bypass.
 return FBoxSphereBounds(Box.ExpandBy(1.f));
}

void URangeAssetTools::PrepareViewBounds()
{
#if WITH_EDITOR
 for(const TCHAR* Directory:{TEXT("Action"),TEXT("Hands")})
 for(const TCHAR* Name:{TEXT("SK_RyanActionArms"),TEXT("SK_M4Action")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/%s.%s"),Directory,Name,Name));
  if(!Mesh)continue;
  Mesh->Modify();
  for(int LOD=0;LOD<Mesh->GetLODNum();++LOD)Mesh->GetLODInfo(LOD)->bAllowCPUAccess=true;
  Mesh->PostEditChange();Mesh->MarkPackageDirty();
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
#endif
}
