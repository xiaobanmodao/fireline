#include "RangeGame.h"
#include "RangeAudio.h"
#include "RangeHitMesh.h"
#include "RangeBallistics.h"
#include "RangeShotFX.h"
#include "RangeCasingFX.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

bool ARangeTarget::TraceShot(const FVector& Start,const FVector& Direction,float MaxDistance,float& Distance,bool& Headshot,FVector& Normal) const
{
 if(GetHealth()<=0||IsHidden())return false;
 // The movement capsule is deliberately excluded. Query the animated visible
 // body, including palms, shoulders, armor edges, neck and feet.
 return CastChecked<URangeHitMesh>(Body)->TraceSurface(Start,Direction,MaxDistance,Distance,Headshot,Normal);
}
float ARangeCharacter::GetHipSpreadAngle() const
{
 if(bKnife||bEmptyHands)return 0;
 const auto& P=RangeHandling::Profile(bMP7);
 return P.HipBase+WeaponHandling[bMP7?1:0].Bloom+RangeHandling::MovementPenalty(GetVelocity().Size2D(),P);
}
float ARangeCharacter::GetShotSpreadAngle() const
{
 // Fully aimed fire retains the existing precise sight ray. Hip bloom is
 // still accumulated while aiming, so releasing ADS does not reset a burst.
 const float Aim=FMath::Clamp(AimAlpha,0.f,1.f);
 return GetHipSpreadAngle()*(1.f-Aim*Aim*(3.f-2.f*Aim));
}
void ARangeCharacter::UpdateShooting(float Dt)
{
 UpdatePlayerVitals(Dt);
 // Exact critically damped spring: stable during uneven frames, settles at zero.
 auto Spring=[Dt](float& X,float& V){const float W=18,E=FMath::Exp(-W*Dt),C=V+W*X;X=(X+C*Dt)*E;V=(V-W*C*Dt)*E;};
 Spring(RecoilPitch,RecoilPitchVelocity);Spring(RecoilYaw,RecoilYawVelocity);
 RecoilPitch=FMath::Clamp(RecoilPitch,0.f,2.f);RecoilYaw=FMath::Clamp(RecoilYaw,-.35f,.35f);
 Camera->SetRelativeRotation(FRotator(RecoilPitch,RecoilYaw,-SlideBlend*.8f));
 FeedbackLeft=FMath::Max(0.f,FeedbackLeft-Dt);
}
void ARangeCharacter::UpdateFiringMovement(float Dt)
{
 // Real emitted rounds own this state. Cooldown gaps must not re-enter sprint;
 // rejected clicks, a knife attack or an empty magazine do not latch it on.
 SprintFireRecovery=FMath::Max(0.f,SprintFireRecovery-Dt);
 bFiringMovement=SprintFireRecovery>0&&!bKnife&&!bEmptyHands&&HandSwitchLeft<=0&&!bReloading;
 auto* Movement=GetCharacterMovement();
 const bool GroundFire=bFiringMovement&&Movement->IsMovingOnGround()&&!bSliding;
 Movement->MaxWalkSpeed=GroundFire||(!bReloading&&bAimHeld)?250.f:(!bReloading&&IsSprintHeld())?700.f:450.f;
 if(bFiringMovement)
 {
  // Ground shooting immediately honors the walking cap, even on the first
  // round. Do not erase jump or slide momentum; retain their movement rules.
  if(Movement->IsMovingOnGround()&&!bSliding)
  {
   const FVector Planar=FVector(Movement->Velocity.X,Movement->Velocity.Y,0).GetClampedToMaxSize(250.f);
   Movement->Velocity.X=Planar.X;Movement->Velocity.Y=Planar.Y;
  }
  ViewGaitTranslation=FVector::ZeroVector;ViewGaitRotation=FQuat::Identity;
 }
}
void ARangeCharacter::Fire()
{
 if(PlayerVitals.Health<=0||bEmptyHands||HandSwitchLeft>0)return;
 if(bKnife){StartKnifeAction(3);return;}
 if(Ammo<=0||IsMP7DrawLocked()||IsM4DrawLocked())return;
 if(bMP7&&MP7ActionBlend>0&&!bMP7ActionReturning)
 {
  const bool ReadyDraw=MP7Action==1;CancelMP7Action();
  if(!ReadyDraw){ReloadFireReturnLeft=.12f;bFireAfterReloadReturn=true;return;}
 }
 ReleaseMP7ReloadRecovery();ReleaseM4ReloadRecovery();
 if(bReloading)
 {
  // Cancel gameplay immediately, then recover the grip before emitting the shot.
  // Retain the last reload sample while its pose fades. Ammo already committed
  // by a seated magazine remains available; cancelling never adds ammunition.
  bReloading=false;bReloadPoseHeld=false;ReloadLeft=0;ReloadLeadOut=0;
  ReloadFireReturnLeft=.12f;bFireAfterReloadReturn=true;
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD cancelled_by_fire ammo=%d"),Ammo);
  return;
 }
 if(ReloadFireReturnLeft>0||FireCooldown>0)return;
 SprintFireRecovery=.22f;UpdateFiringMovement(0.f);
 --Ammo;++Shots;FireCooldown=bMP7?FMath::Max(-.06f,FireCooldown)+.06f:.105f;Kick=1;
 if(!bMP7&&M4Action==1)CancelM4Action();
 LastShotSpread=GetShotSpreadAngle();LastShotAim=Camera->GetForwardVector();
 const FVector Start=Camera->GetComponentLocation(),Direction=RangeHandling::SpreadDirection(Camera->GetComponentQuat(),LastShotSpread,ShotSpreadRandom);
 LastShotDirection=Direction;
 float Distance=15000;FVector Normal=-Direction;bool Impact=false,Head=false;
 FHitResult WorldHit;FCollisionQueryParams Params(SCENE_QUERY_STAT(FirelineFire),true,this);
 if(GetWorld()->LineTraceSingleByChannel(WorldHit,Start,Start+Direction*Distance,ECC_Visibility,Params))
 {Distance=WorldHit.Distance;Normal=WorldHit.ImpactNormal;Impact=true;}
 ARangeTarget* Target=nullptr;
 for(TActorIterator<ARangeTarget> It(GetWorld());It;++It)
 {
  float T;bool H;FVector N;
  if(It->TraceShot(Start,Direction,Distance,T,H,N)){Target=*It;Distance=T;Head=H;Normal=N;Impact=true;}
 }
 // Also respect near cover between the physical muzzle and the sight-line hit.
 auto* Gun=bThirdPerson?BodyGun.Get():ViewGun.Get();
 const FVector Muzzle=Gun->GetSocketLocation(WeaponBone(TEXT("Flash")));
 if(CasingFX)CasingFX->Eject(Gun,bMP7,GetVelocity());
 FVector End=Start+Direction*Distance;FHitResult MuzzleHit;
 if(GetWorld()->LineTraceSingleByChannel(MuzzleHit,Muzzle,End,ECC_Visibility,Params)&&FVector::Distance(MuzzleHit.ImpactPoint,End)>2)
 {Target=nullptr;Head=false;End=MuzzleHit.ImpactPoint;Normal=MuzzleHit.ImpactNormal;Impact=true;}
 LastShotEnd=End;
 const float ShieldBefore=Target?Target->GetShield():0;
 const int Damage=bMP7?(Head?16:11):(Head?RangeDamage::Head:RangeDamage::Body);
 const bool RegisteredHit=Target&&Target->RegisterHit(Damage);
 if(RegisteredHit)
 {
  ++Hits;++BulletHits;Headshots+=Head?1:0;LastDamage=Damage;
  bLastShieldHit=ShieldBefore>0;bLastShieldBreak=bLastShieldHit&&Target->GetShield()<=0;
  if(bLastShieldHit)Target->ShieldImpact(End);bLastHeadshot=Head;bLastKill=Target->GetHealth()==0;
  if(bLastKill)LastKillTTK=Target->GetLastTTK();
  Kills+=bLastKill?1:0;HitFlash=.18f;FeedbackLeft=.75f;
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_HIT head=%d damage=%d health=%d kills=%d"),Head,LastDamage,Target->GetHealth(),Kills);
 }
 CombatAudio->Shot(Muzzle,End,bThirdPerson,Impact&&!Target,RegisteredHit,Head,RegisteredHit&&ShieldBefore>0,RegisteredHit&&bLastShieldBreak);
 auto* FX=GetWorld()->SpawnActor<ARangeShotFX>();FX->Setup(Gun,End,Normal,Impact,Target!=nullptr);
 if(RegisteredHit&&ShieldBefore>0)FX->SetShieldImpact(bLastShieldBreak);
 else if(RegisteredHit)FX->SetFleshImpact();
 const auto& Profile=RangeHandling::Profile(bMP7);auto& Handling=WeaponHandling[bMP7?1:0];
 const float Strength=1.f+Profile.KickGrowth*Handling.Heat(Profile);
 RecoilPitchVelocity+=FMath::Lerp(Profile.HipKick,Profile.ADSKick,AimAlpha)*Strength;
 RecoilYawVelocity+=RangeHandling::Horizontal(Handling.BurstShots,bMP7)*Profile.YawKick*FMath::Lerp(1.f,.65f,AimAlpha);
 // Only actual emitted rounds increase spread/kick. Rejected clicks, draw,
 // inspection and reload never consume this state or the spread RNG.
 Handling.Fired(Profile);
 UE_LOG(LogTemp,Verbose,TEXT("FIRELINE_SHOT ammo=%d hits=%d"),Ammo,Hits);
}
