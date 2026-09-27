#pragma once
#include "CoreMinimal.h"
namespace RangeGamepad
{
inline FVector2D Stick(FVector2D V,float DeadZone,float Exponent=1.f)
{
 if(!FMath::IsFinite(V.X)||!FMath::IsFinite(V.Y))return FVector2D::ZeroVector;
 const double Length=V.Size();if(Length<=DeadZone)return FVector2D::ZeroVector;
 return V/Length*FMath::Pow(FMath::Clamp((Length-DeadZone)/(1.f-DeadZone),0.,1.),Exponent);
}
inline bool Trigger(float Value,bool Held){return Value>(Held?.12f:.2f);}
}
