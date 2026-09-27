#include "RangeGroundTransition.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace
{
float Travel(UAnimSequence* A,float T){return A->EvaluateCurveData(TEXT("FL_Travel"),T);}
float TimeAtDistance(UAnimSequence* A,float D)
{
 float Lo=0,Hi=A->GetPlayLength();for(int I=0;I<18;++I){float M=(Lo+Hi)*.5f;if(Travel(A,M)<D)Lo=M;else Hi=M;}return (Lo+Hi)*.5f;
}
int Cardinal(float Angle){return (FMath::RoundToInt(FMath::UnwindDegrees(Angle)/90.f)+4)%4;}
bool UpperAtTime(UAnimSequence* A,float Time,FTransform (&Upper)[4])
{
 if(!A||!A->GetSkeleton())return false;
 const auto& Ref=A->GetSkeleton()->GetReferenceSkeleton();
 const FName Names[]={TEXT("Torso"),TEXT("Chest"),TEXT("UpperArm_L"),TEXT("UpperArm_R")};
 int32 Indices[4],LastRequired=0;
 for(int I=0;I<4;++I){Indices[I]=Ref.FindBoneIndex(Names[I]);if(Indices[I]==INDEX_NONE)return false;LastRequired=FMath::Max(LastRequired,Indices[I]);}
 TArray<FTransform> Component;Component.SetNumUninitialized(LastRequired+1);
 const FAnimExtractContext Context(static_cast<double>(Time),false);
 for(int I=0;I<=LastRequired;++I)
 {
  // Runtime sequence data, with native local translations/scales and the
  // asset's existing root/pelvis registration. No reference-angle estimate.
  FTransform Local=Ref.GetRefBonePose()[I];
  A->GetBoneTransform(Local,FSkeletonPoseBoneIndex(I),Context,false);
  const int Parent=Ref.GetParentIndex(I);Component[I]=Parent<0?Local:Local*Component[Parent];
 }
 for(int I=0;I<4;++I)Upper[I]=Component[Indices[I]];
 return true;
}
float UpperEntryScore(const FRangeGroundTransition& Transition,UAnimSequence* A,float Time)
{
 FTransform Upper[4];if(!UpperAtTime(A,Time,Upper))return 0.f;
 float Score=0;
 for(int I=0;I<4;++I)Score+=FVector::DistSquared(Upper[I].GetLocation(),Transition.LastUpper[I].GetLocation());
 // Orientation is measured as displacement of three joint-frame probes, in
 // cm like the foot/position score. Probe radius comes from the actual spine
 // segment length, so no hand-picked lean/yaw angle or degree-to-cm constant.
 for(int I=0;I<2;++I)
  for(const FVector Axis:{FVector::XAxisVector,FVector::YAxisVector,FVector::ZAxisVector})
   Score+=FVector::DistSquared(Upper[I].GetRotation().RotateVector(Axis)*Transition.UpperOrientationRadius[I],Transition.LastUpper[I].GetRotation().RotateVector(Axis)*Transition.UpperOrientationRadius[I])/3.f;
 return Score;
}
}
void FRangeGroundTransition::Update(float Dt,float Speed,float Direction,const FVector2D& Desired,bool Grounded,float StopDistance,float DesiredSpeed)
{
 if(!Enabled||Clips.Num()!=32)return;
 const bool Input=Desired.SizeSquared()>1.f;
 const FVector2D Velocity(FMath::Cos(FMath::DegreesToRadians(Direction)),FMath::Sin(FMath::DegreesToRadians(Direction)));
 const float Opposition=Input?FVector2D::DotProduct(Velocity,Desired.GetSafeNormal()):1.f;
 EMode Request=EMode::None;float SelectDirection=Direction;
 if(Grounded)
 {
  if(Input&&!HadInput&&LastSpeed<25.f){Request=EMode::Start;SelectDirection=FMath::RadiansToDegrees(FMath::Atan2(Desired.Y,Desired.X));}
  else if(!Input&&HadInput&&LastSpeed>50.f){Request=EMode::Stop;SelectDirection=LastDirection;}
  else if(FParse::Param(FCommandLine::Get(),TEXT("FirelineAuthoredPivotStudy"))&&Input&&Opposition<-.5f&&Speed>80.f&&Mode!=EMode::Pivot)Request=EMode::Pivot;
 }
 // Four-direction authored clips do not cover a diagonal braking pose.
 if(Request!=EMode::None&&FMath::Abs(FMath::FindDeltaAngleDegrees(SelectDirection,Cardinal(SelectDirection)*90.f))>22.5f)Request=EMode::None;
 if(Request!=EMode::None)
 {
  PreviousClip=Clip;PreviousTime=Time;Mix=Weight>.01f?0.f:1.f;
  Mode=Request;Age=0;EntryDirection=SelectDirection;EntrySpeed=FMath::Max(LastSpeed,Speed);const int Gait=FMath::Max(FMath::Max(LastSpeed,Speed),DesiredSpeed)>260.f?1:0;
  Clip=Gait*12+Cardinal(SelectDirection)*3+(Mode==EMode::Start?0:Mode==EMode::Stop?1:2);
  Time=0;
  if(Mode==EMode::Stop)
  {
   auto* A=Clips[Clip];const float Remaining=StopDistance;
   Time=TimeAtDistance(A,FMath::Max(0.f,Travel(A,A->GetPlayLength())-Remaining));
   float BestScore=FLT_MAX,BestTime=Time;int BestClip=Clip;
   float BestFoot=0,BestUpper=0,FootOnlyScore=FLT_MAX,FootOnlyTime=Time;int FootOnlyClip=Clip;
   const int Candidates[]={Clip,24+Gait*4+Cardinal(SelectDirection)};
   for(int Candidate:Candidates)
   {
    auto* C=Clips[Candidate];const float Match=TimeAtDistance(C,FMath::Max(0.f,Travel(C,C->GetPlayLength())-Remaining));
    for(int Step=-6;Step<=6;++Step)
    {
     const float T=FMath::Clamp(Match+Step/60.f,0.f,C->GetPlayLength());float Score=0;
     const TCHAR* Names[]={TEXT("Foot_L"),TEXT("Foot_R"),TEXT("LowerLeg_L"),TEXT("LowerLeg_R")};
     for(int J=0;J<4;++J){FVector Point;for(int Axis=0;Axis<3;++Axis)Point[Axis]=C->EvaluateCurveData(FName(*FString::Printf(TEXT("FL_%s_%d"),Names[J],Axis)),T);Score+=FVector::DistSquared(Point,LastJoints[J])*(J<2?1.f:.35f);}
     const float FootScore=Score;
     if(FootScore<FootOnlyScore){FootOnlyScore=FootScore;FootOnlyTime=T;FootOnlyClip=Candidate;}
     const float UpperScore=MatchUpperBody?UpperEntryScore(*this,C,T):0.f;
     Score+=UpperScore;
     if(Score<BestScore){BestScore=Score;BestTime=T;BestClip=Candidate;BestFoot=FootScore;BestUpper=UpperScore;}
    }
   }
   Clip=BestClip;Time=BestTime;
   if(MatchUpperBody&&FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace")))
    UE_LOG(LogTemp,Display,TEXT("SOURCE_STOP_ENTRY selected=%s time=%.6f foot=%.3f upper=%.3f total=%.3f footOnly=%s footOnlyTime=%.6f footOnlyScore=%.3f"),*Clips[Clip]->GetName(),Time,BestFoot,BestUpper,BestScore,*Clips[FootOnlyClip]->GetName(),FootOnlyTime,FootOnlyScore);
  }
 }
 Age+=Dt;Mix=FMath::Min(1.f,Mix+Dt/.12f);
 bool Active=Grounded&&Mode!=EMode::None;
 if(Active)
 {
  auto* A=Clips[Clip];const float End=A->GetPlayLength();float Next=Time+Dt;
  if(Speed>10.f)
  {
   Next=Mode==EMode::Stop?TimeAtDistance(A,FMath::Max(0.f,Travel(A,End)-StopDistance)):TimeAtDistance(A,Travel(A,Time)+Speed*Dt/.94f);
   // Bound time advancement; changes in friction or an impact must not seek
   // across the planted foot. This affects animation only, not movement.
   Next=Time+FMath::Clamp(Next-Time,.5f*Dt,1.8f*Dt);
  }
  Time=FMath::Min(Next,End);
  if(Mode==EMode::Start){const float IntendedAngle=FMath::RadiansToDegrees(FMath::Atan2(Desired.Y,Desired.X));Active=Input&&Age<.5f&&FMath::Abs(FMath::FindDeltaAngleDegrees(EntryDirection,IntendedAngle))<70.f;}
  if(Mode==EMode::Stop)Active=!Input&&Age<.5f;
  if(Mode==EMode::Pivot)Active=Input&&Age<.55f;
  Active&=Time<End-.02f;
 }
 if(!Active)Mode=EMode::None;
 Weight=FMath::FInterpConstantTo(Weight,Active?1.f:0.f,Dt,Active?1.f/.12f:1.f/.18f);
 LastSpeed=Speed;if(Speed>10.f)LastDirection=Direction;HadInput=Input;
}
