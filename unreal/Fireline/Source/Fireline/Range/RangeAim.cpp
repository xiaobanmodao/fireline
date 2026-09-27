#include "RangeGame.h"
#include "RangeAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ARangeCharacter::StartAim()
{
 if(bEmptyHands||bKnife)return;
 bAimHeld=true;SlideInputBuffer=0;if(bSliding)EndSlide();
 if(HandSwitchLeft>0||IsMP7DrawLocked()||IsM4DrawLocked())return;
 CancelMP7Action();ReleaseMP7ReloadRecovery();
 if(M4Action==1)CancelM4Action();ReleaseM4ReloadRecovery();
}
void ARangeCharacter::EndAim(){bAimHeld=false;}

void ARangeCharacter::UpdateAim(float Dt)
{
 // Sample the matching neutral clip independently. Weapon draw/reload and
 // a preceding weapon must not contaminate either the sight or hip frame.
 if(!bAimPoseReady && !bEmptyHands && !bKnife && GetGameTimeSinceCreation()>.25f &&
    ViewGun->DoesSocketExist(WeaponBone(TEXT("rearsight"))))
 {
  auto* Calibration=NewObject<USkeletalMeshComponent>(this);
  Calibration->SetSkeletalMesh(ViewGun->GetSkeletalMeshAsset());Calibration->SetVisibility(false);Calibration->SetHiddenInGame(true);Calibration->SetCollisionEnabled(ECollisionEnabled::NoCollision);Calibration->RegisterComponent();
  auto* Hold=LoadObject<UAnimSequence>(nullptr,bMP7?TEXT("/Game/Fireline/MP7/A_MP7_Idle.A_MP7_Idle"):TEXT("/Game/Fireline/SycgffM4/A_M4_Idle.A_M4_Idle"));
  Calibration->PlayAnimation(Hold,false);Calibration->SetPosition(0,false);Calibration->TickAnimation(0,false);Calibration->RefreshBoneTransforms();
  const FVector Rear=Calibration->GetSocketTransform(WeaponBone(TEXT("rearsight")),RTS_Component).GetLocation();
  const FVector Front=Calibration->GetSocketTransform(WeaponBone(TEXT("frontsight")),RTS_Component).GetLocation();
  const FVector Up=Calibration->GetSocketTransform(WeaponBone(TEXT("sightup")),RTS_Component).GetLocation()-Rear;
  AimRotation=FRotationMatrix::MakeFromXZ(Front-Rear,Up).ToQuat().Inverse();
  // 18 cm eye relief; the existing rear aperture and front post meet the camera ray.
  AimLocation=FVector(18,0,0)-AimRotation.RotateVector(Rear*Arms->GetRelativeScale3D());
  IronAimLocation=AimLocation;IronAimRotation=AimRotation;
  if(HasCoyoteSight())
  {
   // Compose in gun space. The child component's world transform can still
   // contain the preceding skeletal pose during this actor's first tick.
   const FTransform Sight=ViewOptic->GetSocketTransform(TEXT("SightCenter"),RTS_Component)*ViewOptic->GetRelativeTransform()*Calibration->GetSocketTransform(WeaponBone(TEXT("body")),RTS_Component);
   CoyoteAimRotation=Sight.GetRotation().Inverse();
   CoyoteAimLocation=FVector(26,0,0)-CoyoteAimRotation.RotateVector(Sight.GetLocation()*Arms->GetRelativeScale3D());
  }
  // A stable hip-fire bore converges on the camera ray at 20 m. Calibrate
  // once in the grip pose, about the muzzle, so a reload never steers itself
  // toward the crosshair or changes weapon placement from frame to frame.
  const FVector Muzzle=Calibration->GetSocketTransform(WeaponBone(TEXT("muzzle")),RTS_Component).GetLocation()*Arms->GetRelativeScale3D();
  const FQuat DefaultHip=FRotator(0,-90,0).Quaternion();
  const FVector HipMuzzle=(bMP7?FVector(10,2,-157):FVector(20,3,-164))+DefaultHip.RotateVector(Muzzle);
  const FQuat TargetFrame=FRotationMatrix::MakeFromXZ(FVector(2000,0,0)-HipMuzzle,FVector::UpVector).ToQuat();
  RifleHipRotation=TargetFrame*AimRotation;
  RifleHipLocation=HipMuzzle-RifleHipRotation.RotateVector(Muzzle);
  if(!bMP7&&MP7GunMesh)
  {
   // Match the MP7's camera-space rear sight and bore direction, not its
   // component offset: the two source rigs and weapon lengths differ.
   Calibration->SetSkeletalMesh(MP7GunMesh);
   Calibration->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/MP7/A_MP7_Idle.A_MP7_Idle")),false);
   Calibration->SetPosition(0,false);Calibration->TickAnimation(0,false);Calibration->RefreshBoneTransforms();
   const FVector MPRear=Calibration->GetSocketTransform(TEXT("MP7_rearsight"),RTS_Component).GetLocation()*.94f;
   const FVector MPFront=Calibration->GetSocketTransform(TEXT("MP7_frontsight"),RTS_Component).GetLocation()*.94f;
   const FVector MPUp=Calibration->GetSocketTransform(TEXT("MP7_sightup"),RTS_Component).GetLocation()*.94f-MPRear;
   const FVector MPMuzzle=Calibration->GetSocketTransform(TEXT("MP7_muzzle"),RTS_Component).GetLocation()*.94f;
   const FVector MPHipMuzzle=FVector(10,2,-157)+DefaultHip.RotateVector(MPMuzzle);
   const FQuat MPFrame=FRotationMatrix::MakeFromXZ(MPFront-MPRear,MPUp).ToQuat();
   const FQuat MPBore=FRotationMatrix::MakeFromXZ(FVector(2000,0,0)-MPHipMuzzle,FVector::UpVector).ToQuat();
   const FQuat MPHip=MPBore*MPFrame.Inverse();
   const FVector RearAnchor=MPHipMuzzle+MPHip.RotateVector(MPRear-MPMuzzle);
   RifleHipRotation=MPBore*IronAimRotation;
   RifleHipLocation=RearAnchor-RifleHipRotation.RotateVector(Rear*Arms->GetRelativeScale3D());
  }
  AimRotation=bCoyoteSight?CoyoteAimRotation:IronAimRotation;
  AimLocation=bCoyoteSight?CoyoteAimLocation:IronAimLocation;
  Calibration->DestroyComponent();bAimPoseReady=true;
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_IRON_SIGHTS ready location=%s rotation=%s"),*AimLocation.ToString(),*AimRotation.Rotator().ToString());
 }
 const bool WantsAim=!bEmptyHands && !bKnife && HandSwitchLeft<=0 && bAimHeld && !bReloading && !IsMP7DrawLocked() && !IsM4DrawLocked() && (MP7ActionBlend<=0||bMP7ActionReturning) && SlideBlend<.1f && bAimPoseReady;
 // The manually selected third-person observer rotates only the camera.
 const bool Directional=FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalLocomotion"));
 GetCharacterMovement()->bUseControllerDesiredRotation=bThirdPerson&&!bThirdPersonOrbit&&(bAimHeld||Directional)&&!bSliding;
 GetCharacterMovement()->bOrientRotationToMovement=bThirdPerson&&!bThirdPersonOrbit&&((!bAimHeld&&!Directional)||bSliding);
 AimAlpha=FMath::FInterpConstantTo(AimAlpha,WantsAim?1.f:0.f,Dt,1.f/.18f);
 const float Blend=AimAlpha*AimAlpha*(3.f-2.f*AimAlpha);
 WalkBobAmount=FMath::FInterpTo(WalkBobAmount,FMath::Clamp((GetCharacterMovement()->IsFalling()||bSliding?0.f:GetVelocity().Size2D())/450.f,0.f,1.f),Dt,8.f);
 WalkBobPhase=FMath::Fmod(WalkBobPhase+Dt*9.f*WalkBobAmount,2.f*PI);
 const bool ContactStudy=WeaponSlot()==1&&FParse::Param(FCommandLine::Get(),TEXT("FirelineContactCarryStudy"));
 const bool SprintStudy=ContactStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineSprintCarryStudy"));
 const float Action=FMath::Max(bMP7?MP7ActionBlend:M4Weight,(bReloading||bReloadPoseHeld||HandSwitchLeft>0)?1.f:0.f);
 const bool CanCarry=SprintStudy&&IsSprintHeld()&&!bKnife&&!bEmptyHands&&!bIsCrouched&&Locomotion.Grounded&&!bSliding&&!bAimHeld&&AimAlpha<.01f&&Action<.01f&&SprintFireRecovery<=0;
 const float CarryTarget=CanCarry?FMath::SmoothStep(460.f,620.f,Locomotion.Speed)*(1.f-FMath::SmoothStep(50.f,75.f,FMath::Abs(Locomotion.DirectionDegrees))):0.f;
 // Exact critically-damped response retains velocity through interrupted transitions.
 const float W=CarryTarget>SprintCarryAlpha?20.f:(bAimHeld||Action>.01f||SprintFireRecovery>0?28.f:18.f),E=FMath::Exp(-W*Dt);
 const float X=SprintCarryAlpha-CarryTarget,C=SprintCarryVelocity+W*X;
 SprintCarryAlpha=FMath::Clamp(CarryTarget+(X+C*Dt)*E,0.f,1.f);
 SprintCarryVelocity=(SprintCarryVelocity-W*C*Dt)*E;
 if(!SprintStudy){SprintCarryAlpha=0;SprintCarryVelocity=0;}
 const bool ReadyStudy=ContactStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedReadyStudy"));
 const float Bob=(ReadyStudy||bFiringMovement)?0.f:FMath::Sin(WalkBobPhase)*WalkBobAmount*.20f*(1.f-Blend);
 // Compose each weapon for the camera. This transforms the complete contact
 // assembly, while dedicated view clips distribute motion through both arms.
 const FVector HipLocation=bKnife?FVector(16,0,-162):(bEmptyHands?FVector(20,3,-164):RifleHipLocation);
 const FQuat HipRotation=(bKnife||bEmptyHands)?FRotator(0,-90,0).Quaternion():RifleHipRotation;
 FVector Position=FMath::Lerp(HipLocation,AimLocation,Blend);
 Position+=FVector(-Kick*FMath::Lerp(1.2f,.35f,Blend),0,Bob-(SlideBlend*2.5f+AirBlend*.65f)*(1.f-Blend));
 Position.Z-=HandLower*(bSwitchFromKnife?60.f:38.f);
 FQuat Rotation=FQuat::Slerp(HipRotation,AimRotation,Blend);
 if(ReadyStudy)
 {
  FTransform Carry;auto* BodyAnim=Cast<URangeAnimInstance>(GetMesh()->GetAnimInstance());
  const bool HasCarry=BodyAnim&&BodyAnim->GetGroundCarry(Carry);
  const float Weight=HasCarry&&!bFiringMovement&&!bKnife&&!bEmptyHands&&!bIsCrouched&&!GetCharacterMovement()->IsFalling()&&!bSliding?
   FMath::SmoothStep(0.f,180.f,Locomotion.Speed)*(1.f-Action)*(1.f-AirBlend)*(1.f-SlideBlend):0.f;
  // Model space is Y-forward; map its source motion to the camera frame.
  const FQuat Frame=GetMesh()->GetRelativeRotation().Quaternion();
  const float Run=SprintStudy?SprintCarryAlpha:0.f;
  FVector Target=HasCarry?Frame.RotateVector(Carry.GetLocation()).GetClampedToMaxSize(SprintStudy?10.f:8.f)*((SprintStudy?FMath::Lerp(.30f,.55f,Run):.25f)*Weight):FVector::ZeroVector;
  // Camera X is depth: ordinary footsteps must not jab the gun at the viewer.
  if(SprintStudy)Target.X*=FMath::Lerp(.15f,1.f,Run);
  FQuat TargetQ=HasCarry?(Frame*Carry.GetRotation()*Frame.Inverse()).GetNormalized():FQuat::Identity;
  const float Angle=TargetQ.AngularDistance(FQuat::Identity);
  TargetQ=FQuat::Slerp(FQuat::Identity,TargetQ,FMath::Min(SprintStudy?FMath::Lerp(.16f,.50f,Run):.16f, FMath::DegreesToRadians(SprintStudy?FMath::Lerp(.5f,5.f,Run):1.5f)/FMath::Max(Angle,.0001f))*Weight).GetNormalized();
  const float Response=1.f-FMath::Exp(-(SprintStudy?FMath::Lerp(10.f,18.f,Run):18.f)*Dt);
  ViewGaitTranslation=bFiringMovement?FVector::ZeroVector:FMath::Lerp(ViewGaitTranslation,Target,Response);
  ViewGaitRotation=bFiringMovement?FQuat::Identity:FQuat::Slerp(ViewGaitRotation,TargetQ,Response).GetNormalized();
  // Rotate about the held assembly, not its distant skeletal origin. Both
  // grips, fingers, gun parts and optic receive exactly the same transform.
  const float HipWeight=1.f-Blend;
  const FQuat Q=FQuat::Slerp(FQuat::Identity,ViewGaitRotation,HipWeight).GetNormalized();
  const FVector Pivot(25,4,-12);
  Position=Pivot+Q.RotateVector(Position-Pivot)+ViewGaitTranslation*HipWeight;
  Rotation=(Q*Rotation).GetNormalized();
 }
 if(SprintStudy&&!bKnife&&!bEmptyHands)
 {
  // Camera composition is deliberately separate from the body's muzzle-up carry.
  // Transform BOTH hands and all weapon parts about the grip, never one wrist.
  const float A=SprintCarryAlpha*(1.f-Blend)*(1.f-Action);
  const FVector Pivot(25,4,-12);
  const FQuat Q=FQuat::Slerp(FQuat::Identity,FRotator(-14.f,-12.f,-18.f).Quaternion(),A);
  Position=Pivot+Q.RotateVector(Position-Pivot)+FVector(-3.f,3.f,-8.f)*A;
  Rotation=(Q*Rotation).GetNormalized();
 }
 if(SightChangeLeft>0)
 {
  SightChangeLeft=FMath::Max(0.f,SightChangeLeft-Dt);
  const float T=1.f-SightChangeLeft/.18f,S=T*T*(3.f-2.f*T);
  Position=FMath::Lerp(SightChangeFrom.GetLocation(),Position,S);
  Rotation=FQuat::Slerp(SightChangeFrom.GetRotation(),Rotation,S);
 }
 Arms->SetRelativeLocation(Position);
 Arms->SetRelativeRotation(Rotation);
 Camera->SetFieldOfView(FMath::Lerp(90.f,74.f,Blend));
}
