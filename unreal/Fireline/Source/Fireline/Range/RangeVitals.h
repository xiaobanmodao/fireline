#pragma once
#include "CoreMinimal.h"
// Shared combat tuning, independent of meshes and animations.
struct FRangeVitals
{
 static constexpr float MaxHealth=100,MaxShield=100,RechargeDelay=5,RechargeRate=20;
 float Health=MaxHealth,Shield=MaxShield,SinceDamage=0;
 bool Damage(float Amount)
 {
  if(Amount<=0||Health<=0)return false;
  SinceDamage=0;const float Absorbed=FMath::Min(Shield,Amount);
  Shield-=Absorbed;Health=FMath::Max(0.f,Health-(Amount-Absorbed));return true;
 }
 void Tick(float Dt)
 {
  if(Health<=0||Dt<=0)return;
  const float Before=SinceDamage;SinceDamage+=Dt;
  const float Active=FMath::Max(0.f,SinceDamage-RechargeDelay)-FMath::Max(0.f,Before-RechargeDelay);
  Shield=FMath::Min(MaxShield,Shield+Active*RechargeRate);
 }
 void Reset(){Health=MaxHealth;Shield=MaxShield;SinceDamage=0;}
};
namespace RangeDamage {inline constexpr int Body=18,Head=27;}
