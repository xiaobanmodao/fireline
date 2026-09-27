#pragma once
#include "CoreMinimal.h"

// First intersection along a finite ray with a capsule, including both caps.
// Skeleton-space hit regions use this rather than the broad movement capsule.
inline bool RangeRayCapsule(const FVector& Origin,const FVector& Direction,float MaxDistance,
 const FVector& A,const FVector& B,float Radius,float& OutDistance)
{
 const FVector BA=B-A,OA=Origin-A;
 const double BB=BA.SizeSquared(),BD=BA|Direction,BO=BA|OA;
 double Best=MaxDistance+1.;
 const FVector Closest=FMath::ClosestPointOnSegment(Origin,A,B);
 if(FVector::DistSquared(Origin,Closest)<=Radius*Radius){OutDistance=0;return true;}
 if(BB>UE_SMALL_NUMBER)
 {
  const double Q=BB-BD*BD,R=BB*(OA|Direction)-BO*BD;
  const double S=BB*OA.SizeSquared()-BO*BO-Radius*Radius*BB;
  const double H=R*R-Q*S;
  if(Q>UE_SMALL_NUMBER&&H>=0)
  {
   const double T=(-R-FMath::Sqrt(H))/Q,Y=BO+T*BD;
   if(T>=0&&Y>=0&&Y<=BB)Best=T;
  }
 }
 for(const FVector& Center:{A,B})
 {
  const FVector OC=Origin-Center;const double D=OC|Direction;
  const double H=D*D-OC.SizeSquared()+Radius*Radius;
  if(H>=0){const double T=-D-FMath::Sqrt(H);if(T>=0)Best=FMath::Min(Best,T);}
 }
 if(Best<=MaxDistance){OutDistance=Best;return true;}return false;
}
