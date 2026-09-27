#include "RangeGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

bool ARangeCharacter::HasCoyoteSight() const
{
 return ViewOptic && ViewOptic->GetStaticMesh() && ViewOptic->DoesSocketExist(TEXT("SightCenter"));
}

void ARangeCharacter::InitializeOptics()
{
 auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Fireline/Optics/SM_CoyoteSight.SM_CoyoteSight"));
 if(!Mesh || !Mesh->FindSocket(TEXT("SightCenter")) || !Mesh->FindSocket(TEXT("RailFromRear")))return;
 // Derive a rigid mount from the selected gun's reference skeleton. Do not
 // sample a live reload to fit the attachment or modify any hand animation.
 const auto& Ref=ViewGun->GetSkeletalMeshAsset()->GetRefSkeleton();
 const int32 RearIndex=Ref.FindBoneIndex(WeaponBone(TEXT("rearsight"))),FrontIndex=Ref.FindBoneIndex(WeaponBone(TEXT("frontsight")));
 const int32 UpIndex=Ref.FindBoneIndex(WeaponBone(TEXT("sightup"))),BodyIndex=Ref.FindBoneIndex(WeaponBone(TEXT("body")));
 if(RearIndex==INDEX_NONE||FrontIndex==INDEX_NONE||UpIndex==INDEX_NONE||BodyIndex==INDEX_NONE)return;
 TArray<FTransform> Bind;
 for(int32 I=0;I<Ref.GetNum();++I){const int32 P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
 const FVector Rear=Bind[RearIndex].GetLocation();
 const FVector Up=(Bind[UpIndex].GetLocation()-Rear).GetSafeNormal();
 const FQuat Frame=FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Bind[FrontIndex].GetLocation()-Rear,Up),Up).ToQuat();
 const FVector Offset=bMP7?FVector(7.89f,0,-2.418f):Mesh->FindSocket(TEXT("RailFromRear"))->RelativeLocation;
 const FTransform Mount(Frame,Rear+Frame.RotateVector(Offset));
 for(auto* Optic:{ViewOptic.Get(),BodyOptic.Get()})
 {
  Optic->SetStaticMesh(Mesh);
  Optic->SetRelativeTransform(Mount.GetRelativeTransform(Bind[BodyIndex]));
 }
 UpdateOpticVisibility();
}

bool ARangeCharacter::SetCoyoteSight(bool Enabled)
{
 if(Enabled&&!HasCoyoteSight())return false;
 if(bCoyoteSight==Enabled)return true;
 bCoyoteSight=Enabled;
 // Keep the two cached aim frames. Selection can happen during a paused
 // reload: recalibrating from that pose would corrupt subsequent ADS.
 if(bAimPoseReady)
 {
  AimLocation=bCoyoteSight?CoyoteAimLocation:IronAimLocation;
  AimRotation=bCoyoteSight?CoyoteAimRotation:IronAimRotation;
 }
 // Blend the complete contact assembly from its actual current placement.
 // Resetting AimAlpha would snap an aimed gun back to the hip for one frame.
 if(AimAlpha>.01f){SightChangeFrom=Arms->GetRelativeTransform();SightChangeLeft=.18f;}
 UpdateOpticVisibility();
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_SIGHT selected=%s"),bCoyoteSight?TEXT("coyote"):TEXT("iron"));
 return true;
}

void ARangeCharacter::UpdateOpticVisibility()
{
 if(!ViewOptic||!BodyOptic)return;
 ViewOptic->SetVisibility(bCoyoteSight&&ViewGun->IsVisible());
 BodyOptic->SetVisibility(bCoyoteSight&&BodyGun->IsVisible());
 BodyOptic->SetOwnerNoSee(!bThirdPerson);
}
