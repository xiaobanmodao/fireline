#pragma once
#include "CoreMinimal.h"

namespace RangeStockContact
{
 // Buttplate centre measured from the selected provider meshes in their
 // native rear/front sight frame (cm). This is a surface point, not gun origin.
 inline FVector Butt(const TArray<FTransform>& Pose,int Rear,int Front,int Up,bool MP7)
 {
  const FVector R=Pose[Rear].GetLocation();
  const FQuat Frame=FRotationMatrix::MakeFromXZ(Pose[Front].GetLocation()-R,Pose[Up].GetLocation()-R).ToQuat();
  return R+Frame.RotateVector(MP7?FVector(-30.47,-.99,-9.76):FVector(-28.96,0,-7.05));
 }
 inline FVector Pocket(const FVector& Shoulder,const FQuat& ChestDeform,const FVector& Forward,const FVector& Right)
 {
  // Front/inside of the right shoulder pad; below the clavicle, away from the
  // humeral hinge. Keep this attachment in the moving chest's anatomical frame.
  return Shoulder+ChestDeform.RotateVector(Forward*6.f-Right*6.f-FVector::UpVector*3.f);
 }
}
