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
    double PoleAngles[2]{},FloorShift=0,Amplitude=.55,LiveRightPoleDelta=0;
    struct FSoleInfluence { int32 Bone; double Weight; FVector Local; };
    TArray<TArray<FSoleInfluence>> SolePoints[2];
    bool IsBelow(int32 Bone,int32 Parent) const;
    bool TwoBone(const FVector& Shoulder,const FVector& Elbow,const FVector& Target,double A,double B,double Phi,FVector& Result) const;
    void Assembly(const TArray<FTransform>& Pose,FQuat& Rotation,FVector& Center,FQuat& Chest) const;
public:
    TArray<FName> Names;
    double LegScale=1,MinReachMargin=0;
    double MaxSoleCorrection=0;
    bool Load(const FString& File);
    bool Evaluate(const TArray<FTransform>& Source,const TArray<FName>& SourceOrder,TArray<FTransform>& Local,TArray<FTransform>& Component,bool LiveClearance);
    bool PlaceSoles(double LeftLock,double RightLock,TArray<FTransform>& Local,TArray<FTransform>& Component);
};
