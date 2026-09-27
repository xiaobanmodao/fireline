#include "RangeGame.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif

void URangeAssetTools::PrepareFullArmProxy()
{
#if WITH_EDITOR
 const FString Folder=TEXT("/Game/Fireline/LocomotionStudy/FullArmSource/");
 auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/SK_RyanRetargetProxy"));check(Source);
 auto Copy=[&](UObject* Original,const FString& Name){
  const FString Path=Folder+Name;if(auto* Existing=LoadObject<UObject>(nullptr,*Path))return Existing;
  return DuplicateObject(Original,CreatePackage(*Path),*Name);
 };
 auto* Mesh=CastChecked<USkeletalMesh>(Copy(Source,TEXT("SK_PhysicalJointProxy")));
 auto* Skeleton=CastChecked<USkeleton>(Copy(Source->GetSkeleton(),TEXT("SKEL_PhysicalJointProxy")));
 Mesh->SetSkeleton(Skeleton);
 FReferenceSkeleton Ref=Source->GetRefSkeleton();TArray<FTransform> World;
 for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);World.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*World[P]);}
 // DJ_forearm is a donor skinning frame offset from the physical elbow.
 // It must not add a segment to normalized FK chain lengths. In this solver
 // proxy ONLY, co-locate it with the real elbow, preserving all wrist/finger
 // component-space reference transforms by compensating child locals.
 for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
 {
  const int E=Ref.FindBoneIndex(*FString::Printf(TEXT("LowerArm_%s"),Side));
  const int H=Ref.FindBoneIndex(*FString::Printf(TEXT("DJ_forearm_%s"),Side));check(E>=0&&H>=0);
  World[H]=World[E];
 }
 {FReferenceSkeletonModifier Edit(Ref,Skeleton);
  for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);Edit.UpdateRefPoseTransform(I,P<0?World[I]:World[I].GetRelativeTransform(World[P]));}
 }
 Mesh->SetRefSkeleton(Ref);Mesh->GetRefBasesInvMatrix().Reset();Mesh->CalculateInvRefMatrices();
 Skeleton->UpdateReferencePoseFromMesh(Mesh);Skeleton->SetPreviewMesh(Mesh);
 for(int I=0;I<Ref.GetNum();++I)Skeleton->SetBoneTranslationRetargetingMode(I,EBoneTranslationRetargetingMode::Animation);
 for(UObject* Asset:{static_cast<UObject*>(Mesh),static_cast<UObject*>(Skeleton)})
 {
  Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  check(UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 }
 UE_LOG(LogTemp,Display,TEXT("FULL_ARM_PHYSICAL_PROXY prepared; original binds untouched"));
#endif
}
