#pragma once
#include "CoreMinimal.h"
class UAnimSequence;
// Authored lower-body transitions; never drives the capsule or blocks input.
struct FRangeGroundTransition
{
 enum class EMode:uint8 {None,Start,Stop,Pivot};
 EMode Mode=EMode::None;
 int Clip=0,PreviousClip=0;
 float Time=0,PreviousTime=0,Weight=0,Mix=1,Age=0,LastSpeed=0,LastDirection=0,EntryDirection=0,EntrySpeed=0;
 bool HadInput=false,Enabled=false;
 // Opt-in SourceBody stop-entry matching against the previous completed mesh
 // pose, in the same component frame as the native candidate animation.
 bool MatchUpperBody=false;
 FVector LastJoints[4]={FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector};
 FTransform LastUpper[4]; // Torso, Chest, UpperArm_L, UpperArm_R (shoulder roots)
 float UpperOrientationRadius[2]={0,0}; // measured Torso->Chest, Chest->Neck
 TArray<UAnimSequence*> Clips;
 void Update(float Dt,float Speed,float Direction,const FVector2D& Desired,bool Grounded,float StopDistance,float DesiredSpeed);
};
