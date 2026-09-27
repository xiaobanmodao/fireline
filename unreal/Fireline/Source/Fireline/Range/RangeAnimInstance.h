#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RangeAnimInstance.generated.h"
struct FRangeGaitCarry;
namespace RangePoseTrace
{
 inline const FName Bones[]={TEXT("Root"),TEXT("Pelvis"),TEXT("Torso"),TEXT("Chest"),TEXT("UpperLeg_L"),TEXT("LowerLeg_L"),TEXT("Foot_L"),TEXT("UpperLeg_R"),TEXT("LowerLeg_R"),TEXT("Foot_R"),TEXT("Shoulder_L"),TEXT("Shoulder_R"),TEXT("UpperArm_L"),TEXT("LowerArm_L"),TEXT("DJ_forearm_L"),TEXT("DJ_wrist_L"),TEXT("UpperArm_R"),TEXT("LowerArm_R"),TEXT("DJ_forearm_R"),TEXT("DJ_wrist_R"),TEXT("M4_rearsight"),TEXT("M4_frontsight"),TEXT("M4_sightup"),TEXT("MP7_rearsight"),TEXT("MP7_frontsight"),TEXT("MP7_sightup"),TEXT("Neck"),TEXT("Head")};
}
struct FRangeBlendSampleTrace
{
 FString Clip;
 float Weight=0,Time=0,PreviousTime=0;
 int PreviousMarker=-2,NextMarker=-2;
};
struct FRangeLocomotionTrace
{
 TArray<FTransform> Ground,Mobility,Layered,Final,BodySolved,ContactClip,ContactRegistered,ContactTargets;
 TArray<FRangeBlendSampleTrace> Samples;
 FVector Weights=FVector::ZeroVector;
 float DirectionalWeight=0,ContactWeight=0;
 FVector FootPlant=FVector::ZeroVector,FootLocked=FVector::ZeroVector;
 FVector ContactMetrics=FVector::ZeroVector;
};
UCLASS(Transient)
class FIRELINE_API URangeAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 TSharedPtr<FRangeGaitCarry> CaptureGait();
 void RestoreGait(const TSharedPtr<FRangeGaitCarry>& Carry);
 FVector GetBodyPoseAuditMetrics();
 FRangeLocomotionTrace GetLocomotionTrace();
 bool GetGroundCarry(FTransform& Out);
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> GroundTransitionAssets;
 UPROPERTY(Transient) TObjectPtr<class UBlendSpace> MovementAsset;
 UPROPERTY(Transient) TObjectPtr<class UBlendSpace> DirectionalAsset;
 UPROPERTY(Transient) TObjectPtr<class UAnimSequence> ReloadAsset;
 UPROPERTY(Transient) TObjectPtr<class UAnimSequence> HoldAsset;
 UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AimAsset;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> MP7Assets;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> M4Assets;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> ContactAssets;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> HandAssets;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> MobilityAssets;
 UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> KnifeAssets;
};
