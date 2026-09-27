#pragma once
#include "CoreMinimal.h"

namespace RangeHandling
{
 // Cone values are half-angles in degrees. These are deliberately forgiving
 // project tuning values, not copied claims about another game's balance.
 struct FProfile
 {
  float HipBase,HipMax,BloomPerShot,RecoveryDelay,RecoveryPerSecond;
  float HipKick,ADSKick,KickGrowth,YawKick;
  float MovementMax;
 };
 inline constexpr FProfile Rifle{.35f,.95f,.055f,.14f,2.2f,32.f,20.f,.30f,3.2f,.60f};
 inline constexpr FProfile MP7{.28f,1.20f,.060f,.12f,2.8f,17.f,11.f,.30f,1.8f,.38f};
 inline const FProfile& Profile(bool IsMP7){return IsMP7?MP7:Rifle;}

 inline float MovementPenalty(float HorizontalSpeed,const FProfile& P)
 {
  // Actual cm/s, shared reference speed: diagonal input, crouch and a wall
  // cannot masquerade as full-speed running. Cap at the current slide limit.
  const float T=FMath::Clamp(HorizontalSpeed/820.f,0.f,1.f);
  return P.MovementMax*T*T*(3.f-2.f*T);
 }

 struct FState
 {
  float Bloom=0,QuietTime=10;
  int32 BurstShots=0;
  void Tick(float Dt,const FProfile& P)
  {
   QuietTime+=Dt;
   // Only the portion after the delay recovers; crossing it at low FPS must
   // not grant a whole extra frame of accuracy recovery.
   Bloom=FMath::Max(0.f,Bloom-P.RecoveryPerSecond*FMath::Clamp(QuietTime-P.RecoveryDelay,0.f,Dt));
   if(QuietTime>.3f)BurstShots=0;
  }
  float Heat(const FProfile& P) const{return FMath::Clamp(Bloom/(P.HipMax-P.HipBase),0.f,1.f);}
  void Fired(const FProfile& P){Bloom=FMath::Min(P.HipMax-P.HipBase,Bloom+P.BloomPerShot);QuietTime=0;++BurstShots;}
 };

 inline FVector SpreadDirection(const FQuat& View,float HalfAngle,FRandomStream& Random)
 {
  if(HalfAngle<=UE_SMALL_NUMBER)return View.GetForwardVector();
  // Uniform area in the cone's tangent-plane disk: no square corners, rings,
  // fixed diagonal bias or dependency on casing/particle random calls.
  const float Radius=FMath::Tan(FMath::DegreesToRadians(HalfAngle))*FMath::Sqrt(Random.FRand());
  const float Angle=2.f*PI*Random.FRand();
  return (View.GetForwardVector()+View.GetRightVector()*(Radius*FMath::Cos(Angle))+View.GetUpVector()*(Radius*FMath::Sin(Angle))).GetSafeNormal();
 }

 inline float Horizontal(int32 BurstShot,bool IsMP7)
 {
  static constexpr float RiflePattern[]={-.2f,.1f,.3f,.4f,.2f,-.1f,-.3f,-.4f,-.2f,0.f,.1f,.1f};
  static constexpr float MP7Pattern[]={.2f,-.15f,.25f,.1f,-.2f,-.25f,-.1f,.15f};
  return IsMP7?MP7Pattern[BurstShot%UE_ARRAY_COUNT(MP7Pattern)]:RiflePattern[BurstShot%UE_ARRAY_COUNT(RiflePattern)];
 }
}
