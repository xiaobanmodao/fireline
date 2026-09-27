#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
class USkeletalMeshComponent;

// Component-space animation, world-space stance anchors. No stretching or
// world access on the animation worker. Only the isolated lateral study uses it.
struct FRangeFootContact
{
 bool CorrectionOnly=false;
 bool Enabled=false,Grounded=false,KeepStationaryPlant=false;
 float AuthoredKneeWeight=0,GaitWeight=0,Speed=0,DeltaTime=1.f/60.f,Plant[2]={0,0};
 FTransform MeshWorld;
 FVector Forward=FVector::YAxisVector;
 struct FFoot{int Upper=-1,Knee=-1,Ankle=-1;bool Locked=false,Releasing=false;float Weight=0;FVector Anchor;};
 FFoot Feet[2];
 int Pelvis=-1;float PelvisDrop=0;
 TArray<int32> Parents;TArray<FTransform> Bind;
 void Initialize(USkeletalMeshComponent* Mesh);
 void Apply(FPoseContext& Pose);
};
