#include "RangePlayerController.h"
#include "RangeGame.h"
#include "RangeLocomotionRules.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

bool ARangeCharacter::IsSprintHeld() const
{
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineRunFireAudit"))){const float T=GetGameTimeSinceCreation()-2.f;const int G=int(T/2.f);const float L=FMath::Fmod(T,2.f);if(G>=44&&G<52)return L>.1f&&L<1.85f;}
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedMovementAudit"))){const float T=GetGameTimeSinceCreation()-2.f;const int G=int(T/2.f);const float L=FMath::Fmod(T,2.f);return G>=24&&G<40&&L>.3f&&L<1.5f;}
 const auto* PC=Cast<ARangePlayerController>(Controller);
 return PC&&(PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift)||PC->PadSprint);
}
void ARangeCharacter::StartSlide()
{
 auto* M=GetCharacterMovement();
 const float Speed=GetVelocity().Size2D();
 if(!RangeLocomotion::CanEnterSlide(M->IsMovingOnGround(),bSliding,bIsCrouched,
       IsSprintHeld(),bReloading,bAimHeld,SlideCooldown,Speed))return;
 bSliding=true;SlideTime=0;SlideInputBuffer=0;ApplyMobilitySettings();
 Locomotion.LastSlideEntrySpeed=RangeLocomotion::SlideEntrySpeed(Speed);
 ++Locomotion.SlideSerial;
 M->Velocity=GetVelocity().GetSafeNormal2D()*Locomotion.LastSlideEntrySpeed;Crouch();
}
void ARangeCharacter::StartCrouch()
{
 bCrouchHeld=true;auto* M=GetCharacterMovement();
 if(M->IsMovingOnGround()&&!bSliding&&!bIsCrouched&&IsSprintHeld()&&!bReloading&&!bAimHeld&&SlideCooldown<=0)
 {
  if(GetVelocity().Size2D()>=RangeLocomotion::SlideMinSpeed)StartSlide();
  else SlideInputBuffer=.25f;
 }
 else if(M->IsMovingOnGround())Crouch();
}
void ARangeCharacter::EndCrouch(){bCrouchHeld=false;SlideInputBuffer=0;if(bSliding)EndSlide();UnCrouch();}
void ARangeCharacter::EndSlide()
{
 if(!bSliding)return;bSliding=false;SlideCooldown=RangeLocomotion::SlideCooldown;ApplyMobilitySettings();
 if(!bCrouchHeld)UnCrouch();
}
void ARangeCharacter::StartJump()
{
 bJumpHeld=true;JumpBuffer=.12f;SlideInputBuffer=0;
 if(bSliding)EndSlide();
}
void ARangeCharacter::EndJump()
{
 bJumpHeld=false;
 // A quick release can arrive between our grounded Jump() request and the
 // movement component accepting it. Preserve that one request, then release
 // immediately after takeoff; otherwise low-FPS buffered taps are lost.
 if(GroundJumpPending>0){bBufferedTapRelease=true;return;}
 StopJumping();
}
void ARangeCharacter::OnJumped_Implementation()
{
 Super::OnJumped_Implementation();
 GroundJumpPending=0;
 ++Locomotion.JumpSerial;
 Locomotion.AirTime=0;
 // A buffered next jump must not carry the previous landing camera dip.
 LandingTime=1;LandingStrength=0;
}
void ARangeCharacter::Landed(const FHitResult& Hit)
{
 Locomotion.LastLandingSpeed=FMath::Max(0.f,-GetVelocity().Z);
 ++Locomotion.LandingSerial;
 LandingTime=0;LandingStrength=FMath::Clamp((Locomotion.LastLandingSpeed-100.f)/500.f,0.f,1.f);
 Super::Landed(Hit);
}
void ARangeCharacter::OnStartCrouch(float HeightAdjust,float ScaledHeightAdjust)
{
 Super::OnStartCrouch(HeightAdjust,ScaledHeightAdjust);
 // Cancel the instantaneous capsule-center shift; only the view eases down.
 StanceEyeZ+=ScaledHeightAdjust;
 if(Boom)Boom->SetRelativeLocation(FVector(0,0,StanceEyeZ));
}
void ARangeCharacter::OnEndCrouch(float HeightAdjust,float ScaledHeightAdjust)
{
 Super::OnEndCrouch(HeightAdjust,ScaledHeightAdjust);
 StanceEyeZ-=ScaledHeightAdjust;
 if(Boom)Boom->SetRelativeLocation(FVector(0,0,StanceEyeZ));
}
void ARangeCharacter::ApplyMobilitySettings()
{
 auto* M=GetCharacterMovement();
 // Input callbacks precede movement simulation: change friction and the speed
 // limit together so the first slide frame cannot brake as ordinary crouch.
 M->GroundFriction=bSliding?RangeLocomotion::SlideFriction:8.f;M->BrakingDecelerationWalking=bSliding?RangeLocomotion::SlideBraking:2048.f;
 M->BrakingFrictionFactor=bSliding?1.f:2.f;M->MaxAcceleration=bSliding?0.f:2048.f;
 M->MaxWalkSpeedCrouched=bSliding?RangeLocomotion::SlideMaxSpeed:180.f;
 if(bSliding)M->MaxWalkSpeed=RangeLocomotion::SlideMaxSpeed;
}
void ARangeCharacter::UpdateLocomotionState(float Dt)
{
 const auto* M=GetCharacterMovement();
 const FVector Velocity=GetVelocity();
 const FVector Local=FRotator(0,GetActorRotation().Yaw,0).UnrotateVector(Velocity);
 Locomotion.LocalVelocity=FVector2D(Local.X,Local.Y);
 Locomotion.Speed=Velocity.Size2D();Locomotion.VerticalSpeed=Velocity.Z;
 const FVector Horizontal(Velocity.X,Velocity.Y,0);
 Locomotion.HorizontalAcceleration=Locomotion.HasVelocitySample&&Dt>SMALL_NUMBER?
  (Horizontal-Locomotion.PreviousHorizontalVelocity)/Dt:FVector::ZeroVector;
 Locomotion.PreviousHorizontalVelocity=Horizontal;Locomotion.HasVelocitySample=true;
 Locomotion.InputAcceleration=M->GetCurrentAcceleration();Locomotion.InputAcceleration.Z=0;
 // Keep the last useful heading at rest, instead of flipping on tiny noise.
 if(Locomotion.Speed>5.f)Locomotion.DirectionDegrees=FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X));
 Locomotion.Grounded=M->IsMovingOnGround();Locomotion.Crouched=bIsCrouched;
 Locomotion.AirTime=M->IsFalling()?Locomotion.AirTime+Dt:0.f;
 Locomotion.Phase=bSliding?ERangeLocomotionPhase::Sliding:Locomotion.Grounded?
  ERangeLocomotionPhase::Grounded:Velocity.Z>80.f?ERangeLocomotionPhase::Rising:
  Velocity.Z<-100.f?ERangeLocomotionPhase::Falling:ERangeLocomotionPhase::Apex;
}
void ARangeCharacter::UpdateMobility(float Dt)
{
 auto* M=GetCharacterMovement();SlideCooldown=FMath::Max(0.f,SlideCooldown-Dt);LandingTime+=Dt;
 if(SlideInputBuffer>0)
 {
  SlideInputBuffer=FMath::Max(0.f,SlideInputBuffer-Dt);
  if(!bCrouchHeld||!IsSprintHeld()||bSliding||bIsCrouched||SlideCooldown>0||bReloading||bAimHeld||!M->IsMovingOnGround())SlideInputBuffer=0;
  else if(GetVelocity().Size2D()>=RangeLocomotion::SlideMinSpeed)StartSlide();
 }
 if(bSliding)
 {
  SlideTime+=Dt;
  if(!M->IsMovingOnGround()||SlideTime>RangeLocomotion::SlideDuration||GetVelocity().Size2D()<RangeLocomotion::SlideExitSpeed)EndSlide();
 }
 ApplyMobilitySettings();
 if(GroundJumpPending>0)
 {
  GroundJumpPending=FMath::Max(0.f,GroundJumpPending-Dt);
  if(GroundJumpPending<=0&&!bJumpHeld){StopJumping();bBufferedTapRelease=false;}
 }
 if(bBufferedTapRelease&&M->IsFalling()){StopJumping();bBufferedTapRelease=false;GroundJumpPending=0;}
 if(JumpBuffer>0)
 {
  JumpBuffer=FMath::Max(0.f,JumpBuffer-Dt);
  if(M->IsMovingOnGround())
  {
   UnCrouch();M->UnCrouch(false); // Expands only if the normal engine headroom test succeeds.
   if(!bIsCrouched){Jump();GroundJumpPending=.12f;JumpBuffer=0;bBufferedTapRelease=!bJumpHeld;}
  }
 }
 else if(M->IsMovingOnGround()&&!bPressedJump)
 {
  if((bCrouchHeld&&SlideInputBuffer<=0)||bSliding)Crouch();else UnCrouch();
 }
 SlideBlend=FMath::FInterpConstantTo(SlideBlend,bSliding?1.f:0.f,Dt,7.f);
 AirBlend=FMath::FInterpConstantTo(AirBlend,M->IsFalling()&&!bIsCrouched?1.f:0.f,Dt,9.f);
 const float TargetEye=bThirdPerson?(bIsCrouched?22.f:30.f)-SlideBlend*8.f:(bIsCrouched?59.f:75.f)-SlideBlend*13.f;
 StanceEyeZ=FMath::FInterpTo(StanceEyeZ,TargetEye,Dt,16);
 Boom->SetRelativeLocation(FVector(0,0,StanceEyeZ));
 const float LandPhase=FMath::Clamp(LandingTime/.26f,0.f,1.f);
 const float Dip=FMath::Square(FMath::Sin(PI*LandPhase))*LandingStrength*2.2f;
 Camera->SetRelativeLocation(FVector(0,0,-Dip));
 UpdateLocomotionState(Dt);
}
