#include "RangeGame.h"
#include "RangeAudio.h"
#include "RangeAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

void ARangeCharacter::KnifeAttackPressed()
{
 // A press/release within one frame must not be lost by held-button polling.
 // The shared action guard rejects the same-frame poll and later repeated clicks.
 // Rifle taps also enter the shared path, including reload cancellation.
 Fire();
}
void ARangeCharacter::StartKnifeAction(int32 Action)
{
 if(!bKnife||HandSwitchLeft>0||bReloading)return;
 if(Action!=2&&Action!=3)return;
 // F restarts inspect, but draw and committed attacks retain their guards.
 if(KnifeAction==1||KnifeAction==3)return;
 if(Action==2&&KnifeAction!=0&&KnifeAction!=2)return;
 // Do not replace a blend with a different single pose. Buffer one higher
 // priority request until the current transition has a definite source sample.
 if(KnifeBlend<1.f){if(Action==3||QueuedKnifeAction!=3)QueuedKnifeAction=Action;return;}
 KnifePreviousAction=KnifeAction;KnifePreviousTime=KnifeTime;
 KnifeAction=Action;KnifeTime=0;KnifeBlend=KnifePreviousAction==0?1.f:0.f;
 bKnifeContactDone=false;QueuedKnifeAction=-1;++KnifeActionSerial;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_ACTION action=%d serial=%d from=%d time=%.4f"),Action,KnifeActionSerial,KnifePreviousAction,KnifePreviousTime);
}
void ARangeCharacter::UpdateKnife(float Dt)
{
 if(!bKnife)return;
 // Freeze the last pose while the old weapon is lowered. Once committed, draw
 // owns its own rise from below the viewport, with no second procedural lift.
 if(HandSwitchLeft>.24f)return;
 KnifeBlend=FMath::Min(1.f,KnifeBlend+Dt/.10f);
 if(KnifeAction!=0)
 {
  const float Before=KnifeTime;KnifeTime+=Dt;CombatAudio->KnifePhase(KnifeAction,Before,KnifeTime,bKarambitReady);
  if(KnifeAction==3&&!bKnifeContactDone&&Before<.22f&&KnifeTime>=.22f)
  {bKnifeContactDone=true;KnifeContact();}
  const auto* Anim=Cast<URangeAnimInstance>(Arms->GetAnimInstance());
  const auto* Clip=Anim&&Anim->KnifeAssets.IsValidIndex(KnifeAction)?Anim->KnifeAssets[KnifeAction].Get():nullptr;
  const float Length=Clip?Clip->GetPlayLength():0.f;
  if(KnifeTime>=Length)
  {
   KnifeAction=0;KnifePreviousAction=0;KnifeTime=0;KnifePreviousTime=0;KnifeBlend=1;
  }
 }
 if(QueuedKnifeAction>=0&&KnifeBlend>=1.f){const int32 Next=QueuedKnifeAction;QueuedKnifeAction=-1;StartKnifeAction(Next);}
}
void ARangeCharacter::KnifeContact()
{
 // A short fan queries the same visible skinned surfaces as bullets. Each ray
 // is clipped by cover and only the nearest result can receive one hit/swing.
 FVector Eye;FRotator Facing;GetActorEyesViewPoint(Eye,Facing);
 if(!bThirdPerson){Eye=Camera->GetComponentLocation();Facing=Camera->GetComponentRotation();}
 float Nearest=150.f;ARangeTarget* Best=nullptr;FVector End=Eye+Facing.Vector()*Nearest;
 bool Cover=false;
 for(float Yaw:{-7.f,0.f,7.f})
 {
  const FVector Direction=FRotator(Facing.Pitch,Facing.Yaw+Yaw,0).Vector();
  float Limit=150.f;FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(FirelineKnife),true,this);
  if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*Limit,ECC_Visibility,Params))
  {Limit=Hit.Distance;if(!Best&&Limit<Nearest){Nearest=Limit;Cover=true;End=Hit.ImpactPoint;}}
  for(TActorIterator<ARangeTarget> It(GetWorld());It;++It)
  {
   float Distance;bool Head;FVector Normal;
   if(It->TraceShot(Eye,Direction,FMath::Min(Limit,Nearest),Distance,Head,Normal))
   {Nearest=Distance;Best=*It;Cover=false;End=Eye+Direction*Distance;}
  }
 }
 const float ShieldBefore=Best?Best->GetShield():0;
 const bool Hit=Best&&Best->RegisterHit(50);
 if(Hit)
 {
  ++Hits;bLastShieldHit=ShieldBefore>0;bLastShieldBreak=bLastShieldHit&&Best->GetShield()<=0;
  if(bLastShieldHit)Best->ShieldImpact(Best->Body->GetSocketLocation(TEXT("Chest")));LastDamage=50;bLastHeadshot=false;bLastKill=Best->GetHealth()==0;
  Kills+=bLastKill?1:0;HitFlash=.18f;FeedbackLeft=.75f;
 }
 CombatAudio->MeleeImpact(End,Hit,Cover&&!Best,Hit&&ShieldBefore>0,Hit&&bLastShieldBreak);
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_CONTACT serial=%d hit=%d cover=%d ammo=%d"),KnifeActionSerial,Hit,Cover,Ammo);
}
void URangeAssetTools::PrepareSelectedM9()
{
#if WITH_EDITOR
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/M9/SK_DU_M9.SK_DU_M9"));check(Mesh);
 for(int LOD=0;LOD<Mesh->GetLODNum();++LOD)Mesh->GetLODInfo(LOD)->bAllowCPUAccess=true;
 Mesh->PostEditChange();Mesh->MarkPackageDirty();
 auto* Skeleton=Mesh->GetSkeleton();check(Skeleton);
 const FName Bone(TEXT("HandHold_R")),Name(TEXT("Required_M9_Grip"));
 check(Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone)!=INDEX_NONE);
 if(!Skeleton->FindSocket(Name)){auto* S=NewObject<USkeletalMeshSocket>(Skeleton);S->SocketName=Name;S->BoneName=Bone;S->bForceAlwaysAnimated=true;Skeleton->Sockets.Add(S);}
 Skeleton->SetBoneTranslationRetargetingMode(Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone),EBoneTranslationRetargetingMode::Animation);
 // Save exactly these two packages. No reference-pose update, no map save.
 for(UObject* Asset:{static_cast<UObject*>(Mesh),static_cast<UObject*>(Skeleton)})
 {
  Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
#endif
}
