#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
class UAnimSequence;
class USkeletalMeshComponent;

// Character-owned clocks survive mesh/weapon swaps. No gameplay input is delayed.
struct FRangeContactClock
{
 double Phase=.173;
 float Weights[3]={1,0,0},Fast[3]={1,0,0};
 float Weight=0,FastWeight=0;
 float DeltaTime=1.f/60.f;
 void Update(float Dt,float Speed,float Run,float Enable);
};

// Isolated M4 study. Existing action poses are evaluated normally; this pass
// restores the accepted full-body carry and constrains contacts across blends.
struct FRangeContactCarry
{
 bool Enabled=false,CompactIdleSupport=false,PreserveGroundSpine=false;
 FRangeContactClock Clock;
 float Pitch=0,StockWeight=0;
 bool StockContact=false;int Rear=-1,Front=-1,SightUp=-1;
 FVector Forward=FVector::YAxisVector,Right=-FVector::XAxisVector;
 TArray<UAnimSequence*> Clips;
 TArray<FTransform> Bind,Native;
 // Read-only diagnostic taps; no additional animation evaluation.
 bool TraceEnabled=false,PreserveLocomotionBase=false;
 TArray<int32> TraceIndices;
 TArray<FTransform> ClipTrace,RegisteredTrace,TargetTrace;
 TArray<int32> Parents;
 int Torso=-1,Chest=-1,Neck=-1,Head=-1,RightWrist=-1;
 struct FArm {int Upper,Elbow,Helper,Wrist,Middle;};
 FArm Arms[2];
 TArray<int32> Contacts;
 FTransform Cycle=FTransform::Identity;
 FVector Metrics=FVector::ZeroVector;
 FVector PreviousPole[2]={FVector::ZeroVector,FVector::ZeroVector};
 bool PoleValid[2]={false,false};
 void Initialize(USkeletalMeshComponent* Mesh,const TArray<UAnimSequence*>& InClips,const TArray<FTransform>& Hold);
 void Apply(FPoseContext& Output);
};
