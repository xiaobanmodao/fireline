#pragma once
#include "CoreMinimal.h"

// Runtime version of the reviewed whole-chain registration. Calibration is
// fixed; no per-frame carry fitting or legacy arm/waist layers are applied.
class FLiveCarryRetarget
{
    TArray<FTransform> Bind, LocalBind, Hold, HoldLocal, SourceBind, RawZero;
    TArray<int32> Parents;
    TArray<FName> SourceNames;
    TMap<FName,int32> TargetIndex,SourceIndex;
    FQuat AssemblyZero;
    FVector CenterZero,HoldMid,HoldSpan,FitTranslation;
    FQuat FitRotation;
    FQuat AimRotation=FQuat::Identity;
    FVector AimTranslation=FVector::ZeroVector;
    double AimPole[2]{};
    FVector ReadyTranslation=FVector::ZeroVector;
    double ReadyPole[2]{};
    double MovingAimPoleDelta[2]{};
    FVector JogTranslation=FVector::ZeroVector;
    double JogPole[2]{};
    double PoleAngles[2]{},FloorShift=0,Amplitude=.55,LiveRightPoleDelta=0;
    struct FSoleInfluence { int32 Bone; double Weight; FVector Local; };
    TArray<TArray<FSoleInfluence>> SolePoints[2];
    bool IsBelow(int32 Bone,int32 Parent) const;
    bool TwoBone(const FVector& Shoulder,const FVector& Elbow,const FVector& Target,double A,double B,double Phi,FVector& Result) const;
    void Assembly(const TArray<FTransform>& Pose,FQuat& Rotation,FVector& Center,FQuat& Chest) const;
public:
    TArray<FName> Names;
    TArray<FTransform> RawMapped;
    double LegScale=1,MinReachMargin=0;
    double MaxSoleCorrection=0;
    double JogBlend=0,PelvisReachOffset=0,TrajectoryReachOffset=0;
    bool CompensateReach=false;
    double ArmedWeight=0,AimWeight=0,MovingWeight=0;
    FVector ViewDirection=FVector(0,1,0);
private:
    bool InReachPass=false;
public:
    bool Load(const FString& File);
    bool LoadAim(const FString& File);
    bool Evaluate(const TArray<FTransform>& Source,const TArray<FName>& SourceOrder,TArray<FTransform>& Local,TArray<FTransform>& Component,bool LiveClearance);
    bool PlaceSoles(double LeftLock,double RightLock,TArray<FTransform>& Local,TArray<FTransform>& Component);
};
