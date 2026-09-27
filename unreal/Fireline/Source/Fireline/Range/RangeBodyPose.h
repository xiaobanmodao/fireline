#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
class USkeletalMeshComponent;
class UAnimSequence;

// Full-body control after action blending. View meshes never enter this pass.
// Hand/finger/weapon contact assemblies remain rigid; UE two-bone IK solves arms.
struct FRangeBodyPose
{
 bool Enabled=false,MP7=false,Audit=false,StockContact=false;
 bool ContinuousContact=false;
 float StockWeight=0;
 float Aim=0,Pitch=0,Yaw=0,Weight=1,Reload=0;
 float SourceBodyWeight=0;
 bool SourceBodyConstraints=false;
 float SourcePostureWeight=0,SourceSpineLow=0,SourceSpineHigh=0,SourceBodyLean=0,SourceLeanScale=1;
 float GroundChestWeight=0,FrameDeltaSeconds=1.f/60.f;
 float StrafeRoll=0,StrafeRollWeight=0;
 bool CoherentGroundFit=false;
 bool StableCarryElbows=false,SprintCarryStudy=false;
 mutable float PreviousSprintCarry=0;
 mutable bool HasElbowHistory=false;
 mutable FVector PreviousElbowChest[2];
 mutable bool HasContactHistory=false;
 mutable FVector PreviousContactChest=FVector::ZeroVector;
 mutable FVector PreviousFitChest=FVector::ZeroVector;
 // Cross-weapon state contains chest-space physical points, never old bind indices.
 mutable bool HasTransferHistory=false,PendingWeaponTransfer=false,WeaponTransferActive=false;
 mutable FVector PreviousWristChest[2];
 mutable float WeaponTransferTime=0;
 mutable FVector WeaponTransferShiftChest=FVector::ZeroVector;
 mutable FQuat WeaponTransferRotationChest=FQuat::Identity;
 float CarryWeight=0,SprintCarry=0;
 FVector CarryTranslation=FVector::ZeroVector;
 FQuat CarryRotation=FQuat::Identity;
 mutable FVector FitShift=FVector::ZeroVector,AuditMetrics=FVector::ZeroVector;
 mutable TArray<FVector> FitKey;
 mutable bool HasFit=false;
 FVector Forward=FVector::YAxisVector,Right=-FVector::XAxisVector,Up=FVector::UpVector;
 TArray<FTransform> Bind,Neutral;
 TArray<int32> Parents;
 TArray<FName> Names;
 int32 Torso=-1,Chest=-1,Neck=-1,Head=-1,Rear=-1,Front=-1,SightUp=-1;
 struct FArm{int32 Clavicle=-1,Upper=-1,Lower=-1,Forearm=-1,Wrist=-1,Middle=-1;float Side=0;TArray<int32> HandBones;};
 FArm Arm[2];
 void Initialize(USkeletalMeshComponent* Mesh,UAnimSequence* Hold,bool IsMP7);
 void Apply(FPoseContext& Output) const;
};
