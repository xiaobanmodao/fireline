#include "RangeHitMesh.h"
#include "RangeGame.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

void URangeHitMesh::BeginPlay()
{
 Super::BeginPlay();
 const auto* Mesh=GetSkeletalMeshAsset();
 const auto* Render=Mesh?Mesh->GetResourceForRendering():nullptr;
 if(!Render||Render->LODRenderData.IsEmpty())return;
 const auto& LOD=Render->LODRenderData[0];
 const auto& Weights=LOD.SkinWeightVertexBuffer;
 const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
 if(!Indices||!LOD.StaticVertexBuffers.PositionVertexBuffer.GetVertexData()||!Weights.GetDataVertexBuffer()->GetWeightData())
 {UE_LOG(LogTemp,Error,TEXT("FIRELINE_HIT_MESH missing CPU buffers; run prepare_hit_mesh.py"));return;}
 const auto& Ref=Mesh->GetRefSkeleton();
 const int HeadBone=Ref.FindBoneIndex(TEXT("Head"));
 if(HeadBone==INDEX_NONE){UE_LOG(LogTemp,Error,TEXT("FIRELINE_HIT_MESH missing Head bone"));return;}
 HeadWeights.Init(0,LOD.GetNumVertices());
 DominantBones.Init(0,LOD.GetNumVertices());
 TArray<uint16> MaxWeights;MaxWeights.Init(0,LOD.GetNumVertices());
 for(const auto& Section:LOD.RenderSections)
 {
  for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
   for(uint32 I=0;I<Weights.GetMaxBoneInfluences();++I)
   {
    const uint16 W=Weights.GetBoneWeight(V,I);if(!W)continue;
    int Bone=Section.BoneMap[Weights.GetBoneIndex(V,I)];
    if(W>MaxWeights[V]){MaxWeights[V]=W;DominantBones[V]=Bone;}
    while(Bone!=INDEX_NONE&&Bone!=HeadBone)Bone=Ref.GetParentIndex(Bone);
    if(Bone==HeadBone)HeadWeights[V]+=float(W)/65535.f;
   }
  for(uint32 T=0;T<Section.NumTriangles;++T)
  {
   const uint32 I=Section.BaseIndex+T*3;
   Triangles.Add({Indices->Get(I),Indices->Get(I+1),Indices->Get(I+2)});
  }
 }
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_HIT_MESH vertices=%d triangles=%d legacy_chest_bone=%d torso_bone=%d"),HeadWeights.Num(),Triangles.Num(),Ref.FindBoneIndex(TEXT("Chest")),Ref.FindBoneIndex(TEXT("Torso")));
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_HIT_BONES old_arm=%d imported_arm=%d old_hand=%d imported_hand=%d"),Ref.FindBoneIndex(TEXT("UpperArm.L")),Ref.FindBoneIndex(TEXT("UpperArm_L")),Ref.FindBoneIndex(TEXT("UpperHand.L")),Ref.FindBoneIndex(TEXT("Palm_L"))!=INDEX_NONE?Ref.FindBoneIndex(TEXT("Palm_L")):Ref.FindBoneIndex(TEXT("UpperHand_L")));
}
bool URangeHitMesh::UpdateQueryPose() const
{
 if(Triangles.IsEmpty())return false;
 const uint32 Revision=GetBoneTransformRevisionNumber();
 if(PoseFrame==GFrameCounter&&PoseRevision==Revision)return true;
 auto* Self=const_cast<URangeHitMesh*>(this);
 const auto& LOD=GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
 GetCurrentRefToLocalMatrices(Matrices,0);
 // Positions only: no GPU readback, render-thread flush, or CPU-rendering switch.
 ComputeSkinnedPositions(Self,Positions,Matrices,LOD,LOD.SkinWeightVertexBuffer);
 PoseBox=FBox(ForceInit);for(const auto& P:Positions)PoseBox+=FVector(P);
 PoseFrame=GFrameCounter;PoseRevision=Revision;return true;
}
bool URangeHitMesh::TraceSurface(const FVector& Start,const FVector& Direction,float Limit,float& Distance,bool& Headshot,FVector& Normal) const
{
 if(!UpdateQueryPose())return false;
 const FTransform World=GetComponentTransform();
 const FVector O=World.InverseTransformPosition(Start),D=World.InverseTransformVector(Direction);
 // D deliberately retains scale: the ray parameter remains a world distance.
 if(!FMath::LineBoxIntersection(PoseBox.ExpandBy(.01),O,O+D*Limit,D*Limit))return false;
 double Nearest=Limit;bool Found=false;
 for(const auto& T:Triangles)
 {
  const FVector A(Positions[T.A]),E1=FVector(Positions[T.B])-A,E2=FVector(Positions[T.C])-A;
  const FVector P=D^E2;const double Det=E1|P;
  if(FMath::Abs(Det)<1.e-9)continue;
  const FVector S=O-A;const double U=(S|P)/Det;
  if(U< -1.e-7||U>1.0000001)continue;
  const FVector Q=S^E1;const double V=(D|Q)/Det;
  if(V< -1.e-7||U+V>1.0000001)continue;
  const double RayT=(E2|Q)/Det;
  if(RayT<0||RayT>Nearest)continue;
  Nearest=RayT;Found=true;
  const uint32 Vertex=U>V&&U>1-U-V?T.B:V>1-U-V?T.C:T.A;
  LastHitBone=GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(DominantBones[Vertex]);
  Headshot=HeadWeights[T.A]*(1-U-V)+HeadWeights[T.B]*U+HeadWeights[T.C]*V>=.5;
  Normal=World.TransformVectorNoScale((E1^E2).GetSafeNormal()).GetSafeNormal();
  if((Normal|Direction)>0)Normal=-Normal;
 }
 if(Found)Distance=float(Nearest);
 return Found;
}

void URangeAssetTools::PrepareHitMesh()
{
#if WITH_EDITOR
 for(const TCHAR* Directory:{TEXT("Action"),TEXT("Hands"),TEXT("Drazen"),TEXT("DrazenOriginal")})
 {
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/SK_RyanAction.SK_RyanAction"),Directory));
 if(!Mesh)continue;
 Mesh->Modify();
 for(int LOD=0;LOD<Mesh->GetLODNum();++LOD)Mesh->GetLODInfo(LOD)->bAllowCPUAccess=true;
 Mesh->PostEditChange();Mesh->MarkPackageDirty();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
#endif
}
