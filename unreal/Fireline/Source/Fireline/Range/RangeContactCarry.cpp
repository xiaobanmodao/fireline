#include "RangeContactCarry.h"
#include "RangeAnimInstance.h"
#include "RangeStockContact.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void FRangeContactClock::Update(float Dt,float Speed,float Run,float Enable)
{
 const float Moving=FMath::SmoothStep(0.f,100.f,Speed);
 const float Target[3]={1-Moving,Moving*(1-Run),Moving*Run};
 // Exact persistent two-pole filter, shared with the accepted transition study.
 const float D=FMath::Clamp(Dt,0.f,.1f),E=FMath::Exp(-22.f*D);
 DeltaTime=D;
 for(int I=0;I<3;++I){Weights[I]=Target[I]+(Weights[I]-Target[I]+(Fast[I]-Target[I])*22.f*D)*E;Fast[I]=Target[I]+(Fast[I]-Target[I])*E;}
 Weight=Enable+(Weight-Enable+(FastWeight-Enable)*22.f*D)*E;FastWeight=Enable+(FastWeight-Enable)*E;
 // Median backward foot speed during low-foot source samples, including the
 // real .94 mesh scale: Walk 298.8375, Jog 622.6303 cm/s. These clips contain
 // multiple strides; their common duration is not one gait cycle.
 const float MovingWeight=Weights[1]+Weights[2];
 const double NativeSpeed=MovingWeight>.0001f?(298.8375*Weights[1]+622.6303*Weights[2])/MovingWeight:298.8375;
 Phase=FMath::Fmod(Phase+MovingWeight*FMath::Clamp(double(Speed)/NativeSpeed,0.,2.)*D/(46./30.),1.);
}
namespace
{
 bool Child(int I,int P,const TArray<int32>& Parents){for(;I>=0;I=Parents[I])if(I==P)return true;return false;}
 FTransform Mix(const FTransform& A,const FTransform& B,float W){FTransform T;T.Blend(A,B,W);T.NormalizeRotation();return T;}
 void World(FPoseContext& P,const TArray<int32>& Parents,TArray<FTransform>& Out)
 {
  const auto& C=P.Pose.GetBoneContainer();Out.SetNum(Parents.Num());
  for(int I=0;I<Parents.Num();++I){const auto B=C.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));Out[I]=B.GetInt()>=0?P.Pose[B]:FTransform::Identity;if(Parents[I]>=0)Out[I]*=Out[Parents[I]];}
 }
}
void FRangeContactCarry::Initialize(USkeletalMeshComponent* Mesh,const TArray<UAnimSequence*>& InClips,const TArray<FTransform>& Hold)
{
 StockContact=FParse::Param(FCommandLine::Get(),TEXT("FirelineLocomotionContactFixStudy"));Enabled=false;Clips=InClips;if(Clips.Num()!=3||Clips.Contains(nullptr))return;
 Forward=Mesh->GetRelativeRotation().UnrotateVector(FVector::ForwardVector);
 Right=Mesh->GetRelativeRotation().UnrotateVector(FVector::RightVector);
 PoleValid[0]=PoleValid[1]=false;
 const auto& R=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();Bind=R.GetRefBonePose();Native=Hold;
 if(Native.Num()!=Bind.Num())return;
 for(int I=0;I<Bind.Num();++I){Parents.Add(R.GetParentIndex(I));if(Parents[I]>=0)Bind[I]*=Bind[Parents[I]];}
 PreserveLocomotionBase=FParse::Param(FCommandLine::Get(),TEXT("FirelineCarryBaseStudy"));
 TraceEnabled=FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace"));
 TraceIndices.Reset();if(TraceEnabled)for(FName Name:RangePoseTrace::Bones)TraceIndices.Add(R.FindBoneIndex(Name));
 Rear=R.FindBoneIndex(TEXT("M4_rearsight"));Front=R.FindBoneIndex(TEXT("M4_frontsight"));SightUp=R.FindBoneIndex(TEXT("M4_sightup"));
 Torso=R.FindBoneIndex(TEXT("Torso"));Chest=R.FindBoneIndex(TEXT("Chest"));Neck=R.FindBoneIndex(TEXT("Neck"));Head=R.FindBoneIndex(TEXT("Head"));RightWrist=R.FindBoneIndex(TEXT("DJ_wrist_R"));
 for(int I=0;I<2;++I){const TCHAR* S=I?TEXT("R"):TEXT("L");auto Id=[&](const TCHAR* N){return R.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),N,S));};Arms[I]={Id(TEXT("UpperArm")),Id(TEXT("LowerArm")),Id(TEXT("DJ_forearm")),Id(TEXT("DJ_wrist")),Id(TEXT("DJ_middle_01"))};}
 for(int I=0;I<Bind.Num();++I)if(Child(I,Arms[0].Wrist,Parents)||Child(I,RightWrist,Parents)||R.GetBoneName(I).ToString().StartsWith(TEXT("M4_")))Contacts.AddUnique(I);
 Enabled=Torso>=0&&Chest>=0&&Neck>=0&&Head>=0&&RightWrist>=0;
}
void FRangeContactCarry::Apply(FPoseContext& Output)
{
 Cycle=FTransform::Identity;Metrics=FVector::ZeroVector;
 ClipTrace.Reset();RegisteredTrace.Reset();TargetTrace.Reset();
 auto Trace=[&](const TArray<FTransform>& P,TArray<FTransform>& Out){if(TraceEnabled)for(int I:TraceIndices)Out.Add(P.IsValidIndex(I)?P[I]:FTransform::Identity);};
 if(!Enabled||Clock.Weight<.0001f){PoleValid[0]=PoleValid[1]=false;return;}
 FPoseContext Idle(Output),Walk(Output),Jog(Output),BaseWalk(Output),BaseJog(Output),Original(Output);
 auto Sample=[](UAnimSequence* Clip,double Time,FPoseContext& P){P.ResetToRefPose();FAnimationPoseData Data(P);Clip->GetAnimationPose(Data,FAnimExtractContext(Time,false));};
 Sample(Clips[0],0,Idle);Sample(Clips[1],Clock.Phase*Clips[1]->GetPlayLength(),Walk);Sample(Clips[2],FMath::Fmod(Clock.Phase+7./46.,1.)*Clips[2]->GetPlayLength(),Jog);
 Sample(Clips[1],0,BaseWalk);Sample(Clips[2],0,BaseJog);
 Sample(Clips[0],0,Original);
 TArray<FTransform> B,W,J,BW,BJ;World(Output,Parents,B);World(Walk,Parents,W);World(Jog,Parents,J);World(BaseWalk,Parents,BW);World(BaseJog,Parents,BJ);
 const FVector Delta=(W[RightWrist].GetLocation()-BW[RightWrist].GetLocation())*Clock.Weights[1]+(J[RightWrist].GetLocation()-BJ[RightWrist].GetLocation())*Clock.Weights[2];
 const FQuat QW=(W[RightWrist].GetRotation()*BW[RightWrist].GetRotation().Inverse()).GetNormalized(),QJ=(J[RightWrist].GetRotation()*BJ[RightWrist].GetRotation().Inverse()).GetNormalized();
 Cycle=FTransform(FQuat::Slerp(FQuat::Slerp(FQuat::Identity,QW,Clock.Weights[1]),QJ,Clock.Weights[2]).GetNormalized(),Delta);
 const float Moving=Clock.Weights[1]+Clock.Weights[2];
 // The imported FK clips contain a fixed, excessive spine pitch: even their
 // first frames put the head well ahead of the pelvis. Re-register the entire
 // torso chain against the selected native M4 hold, retaining each clip's
 // keyed variation. Keep the original common hand/gun frame for weapon aim;
 // the fixed-length arms below then meet that frame from the revised shoulders.
 for(FCompactPoseBoneIndex I:Original.Pose.ForEachBoneIndex())Original.Pose[I]=Mix(Original.Pose[I],Mix(Walk.Pose[I],Jog.Pose[I],Moving>.0001f?Clock.Weights[2]/Moving:0.f),Moving);
 TArray<FTransform> OriginalC;World(Original,Parents,OriginalC);Trace(OriginalC,ClipTrace);
 const auto& Bones=Output.Pose.GetBoneContainer();
 auto RegisterSpine=[&](FPoseContext& Pose,const FPoseContext& Baseline,float TorsoAmount,float ChestAmount)
 {
  for(const auto [Bone,Amount]:{TPair<int,float>(Torso,TorsoAmount),TPair<int,float>(Chest,ChestAmount),TPair<int,float>(Neck,1.f),TPair<int,float>(Head,1.f)})
  {
   const auto K=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bone));if(K.GetInt()<0)continue;
   const FQuat NativeLocal=Native[Bone].GetRelativeTransform(Native[Parents[Bone]]).GetRotation();
   const FQuat SourceLocal=Pose.Pose[K].GetRotation(),SourceRest=Baseline.Pose[K].GetRotation();
   Pose.Pose[K].SetRotation(FQuat::Slerp(SourceLocal,(NativeLocal*SourceRest.Inverse()*SourceLocal).GetNormalized(),Amount).GetNormalized());
  }
 };
 // The mesh bind wrist frames differ from the source skeleton reference; use
 // mesh-bind reach and wrist feasibility, not skeleton-only measurements.
 // A running rifle carry stays farther forward than a walk.
 RegisterSpine(Idle,Idle,.30f,.75f);RegisterSpine(Walk,BaseWalk,.25f,.20f);RegisterSpine(Jog,BaseJog,.10f,0.f);
 for(FCompactPoseBoneIndex I:Idle.Pose.ForEachBoneIndex())Idle.Pose[I]=Mix(Idle.Pose[I],Mix(Walk.Pose[I],Jog.Pose[I],Moving>.0001f?Clock.Weights[2]/Moving:0.f),Moving);
 TArray<FTransform> C;World(Idle,Parents,C);
 // Rotate a complete torso branch so shoulder/arm motion stays coordinated.
 const FQuat PitchQ(-FVector::XAxisVector,FMath::DegreesToRadians(-Pitch));const FVector Pivot=C[Torso].GetLocation();
 for(int I=0;I<C.Num();++I)if(Child(I,Torso,Parents)){C[I].SetLocation(Pivot+PitchQ.RotateVector(C[I].GetLocation()-Pivot));C[I].SetRotation((PitchQ*C[I].GetRotation()).GetNormalized());}
 // A rifle gait can lean without carrying its full lateral roll into the
 // helmet. Compensate at the neck/head end of the chain; this does not move a
 // shoulder, hand, or gun contact away from the selected source frame.
 const FVector HeadUp=(C[Head].GetRotation()*Native[Head].GetRotation().Inverse()).RotateVector(FVector::UpVector);
 const float HeadRoll=FMath::Atan2(HeadUp.X,HeadUp.Z);
 const float SafeRoll=FMath::Clamp(HeadRoll,FMath::DegreesToRadians(-6.f),FMath::DegreesToRadians(6.f));
 C[Head].SetRotation((FQuat(FVector::YAxisVector,SafeRoll-HeadRoll)*C[Head].GetRotation()).GetNormalized());
 if(PreserveGroundSpine)
 {
  const auto Donor=C;
  for(int I=0;I<C.Num();++I)
  {
   if(!Child(I,Torso,Parents)||I==Torso||I==Chest||I==Neck||I==Head)C[I]=B[I];
   else C[I]=Donor[I].GetRelativeTransform(Donor[Parents[I]])*C[Parents[I]];
  }
 }
 // The carry layer owns the torso/arms, not the locomotion root or legs.
 // Register its complete upper-body branch onto the evaluated pelvis without
 // inheriting a second pelvis yaw. The torso attachment keeps its authored local
 // offset; both hands and the weapon receive the SAME translation below.
 FVector BaseShift=FVector::ZeroVector;
 if(PreserveLocomotionBase)
 {
  const int Parent=Parents[Torso];
  const FVector LocalAttachment=C[Torso].GetRelativeTransform(C[Parent]).GetLocation();
  BaseShift=B[Parent].TransformPosition(LocalAttachment)-C[Torso].GetLocation();
  for(int I=0;I<C.Num();++I)
   if(Child(I,Torso,Parents))C[I].AddToTranslation(BaseShift);
   else C[I]=B[I];
 }
 Trace(C,RegisteredTrace);
 const FVector OriginalPivot=OriginalC[Torso].GetLocation();
 FTransform OriginalAnchor=OriginalC[RightWrist];
 OriginalAnchor.SetLocation(OriginalPivot+PitchQ.RotateVector(OriginalAnchor.GetLocation()-OriginalPivot));
 OriginalAnchor.SetRotation((PitchQ*OriginalAnchor.GetRotation()).GetNormalized());
 OriginalAnchor.AddToTranslation(BaseShift);
 if(CompactIdleSupport)
 {
  // The donor idle holds the support wrist almost a full arm-length from its
  // shoulder. Shorten the whole rifle/hand assembly along body forward until
  // the fixed-length elbow circle has room for an actual supported bend.
  const auto& A=Arms[0];
  const FVector SupportWrist=(Native[A.Wrist].GetRelativeTransform(Native[RightWrist])*OriginalAnchor).GetLocation();
  const FVector Reach=SupportWrist-C[A.Upper].GetLocation();
  const double MaxReach=.8*((Bind[A.Elbow].GetLocation()-Bind[A.Upper].GetLocation()).Size()+(Bind[A.Wrist].GetLocation()-Bind[A.Elbow].GetLocation()).Size());
  const double Along=Reach.Dot(Forward);
  const double AcrossSquared=FMath::Max(0.,Reach.SizeSquared()-Along*Along);
  if(Along>0&&AcrossSquared<MaxReach*MaxReach)
  {
   const double Pull=FMath::Clamp(Along-FMath::Sqrt(MaxReach*MaxReach-AcrossSquared),0.,18.);
   OriginalAnchor.AddToTranslation(-Forward*Pull*Clock.Weights[0]);
  }
 }
 for(int I:Contacts)C[I]=Native[I].GetRelativeTransform(Native[RightWrist])*OriginalAnchor;
 if(StockContact&&Rear>=0&&StockWeight>0)
 {
  const FQuat ChestDeform=(C[Chest].GetRotation()*Bind[Chest].GetRotation().Inverse()).GetNormalized();
  const FVector StockDelta=(RangeStockContact::Pocket(C[Arms[1].Upper].GetLocation(),ChestDeform,Forward,Right)-RangeStockContact::Butt(C,Rear,Front,SightUp,false))*StockWeight;
  for(int I:Contacts)C[I].AddToTranslation(StockDelta);
 }
 // Blend body locals; blend the entire contact frame separately. In particular,
 // don't blend each hand through a different shoulder and lose the grip.
 TArray<FTransform> P;P.SetNum(C.Num());const auto& Container=Output.Pose.GetBoneContainer();
 for(int I=0;I<P.Num();++I){const auto K=Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));const FTransform Local=Parents[I]>=0?C[I].GetRelativeTransform(C[Parents[I]]):C[I];P[I]=K.GetInt()>=0?Mix(Output.Pose[K],Local,Clock.Weight):Local;if(Parents[I]>=0)P[I]*=P[Parents[I]];}
 const FTransform Anchor=Mix(B[RightWrist],C[RightWrist],Clock.Weight);
 for(int I:Contacts)P[I]=Mix(B[I].GetRelativeTransform(B[RightWrist]),C[I].GetRelativeTransform(C[RightWrist]),Clock.Weight)*Anchor;
 // Shared translation for reach: no stretching and no separate hand offsets.
 for(int Pass=0;Pass<8;++Pass)for(const auto& A:Arms){const double Max=(Bind[A.Elbow].GetLocation()-Bind[A.Upper].GetLocation()).Size()+(Bind[A.Wrist].GetLocation()-Bind[A.Elbow].GetLocation()).Size()-.02;const FVector D=P[A.Wrist].GetLocation()-P[A.Upper].GetLocation();if(D.Size()>Max)for(int I:Contacts)P[I].AddToTranslation(D.GetSafeNormal()*(Max-D.Size()));}
 Trace(P,TargetTrace);
 for(int ArmIndex=0;ArmIndex<2;++ArmIndex)
 {
  const auto& A=Arms[ArmIndex];
  const FVector S=P[A.Upper].GetLocation(),T=P[A.Wrist].GetLocation(),OldE=P[A.Elbow].GetLocation();
  const double L1=(Bind[A.Elbow].GetLocation()-Bind[A.Upper].GetLocation()).Size(),L2=(Bind[A.Wrist].GetLocation()-Bind[A.Elbow].GetLocation()).Size(),D=(T-S).Size();
  const FVector Axis=(T-S).GetSafeNormal(),Center=S+Axis*((L1*L1-L2*L2+D*D)/(2*FMath::Max(D,.001)));
  const double Radius=FMath::Sqrt(FMath::Max(0.,L1*L1-(Center-S).SizeSquared()));
  FVector Pole=FVector::VectorPlaneProject(OldE-S,Axis).GetSafeNormal();
  if(Pole.IsNearlyZero())Pole=FVector::VectorPlaneProject(-FVector::UpVector,Axis).GetSafeNormal();
  if(CompactIdleSupport&&ArmIndex==0)
  {
   // Use the support arm's down/out bend plane at idle. The wrist cone below
   // still limits this pole, and the phase-history limiter keeps exits smooth.
   const FVector Supported=FVector::VectorPlaneProject(Forward*5.f-Right*25.f-FVector::UpVector*20.f,Axis).GetSafeNormal();
   if(!Supported.IsNearlyZero())Pole=FMath::Lerp(Pole,Supported,FMath::Clamp(Clock.Weights[0],0.f,1.f)).GetSafeNormal();
  }
  const FQuat Deform=(P[A.Wrist].GetRotation()*Bind[A.Wrist].GetRotation().Inverse()).GetNormalized();
  const FVector Rest=(Bind[A.Wrist].GetLocation()-Bind[A.Elbow].GetLocation()).GetSafeNormal(),ForearmDesired=Deform.RotateVector(Rest);
  // Constrain the actual palm axis, not the forearm bind axis: the native
  // hand has its own rest offset and the two differ under the new spine pose.
  const FVector Desired=PreserveGroundSpine&&A.Middle>=0?(P[A.Middle].GetLocation()-T).GetSafeNormal():ForearmDesired;
  FVector Elbow=Center+Pole*Radius;double Bend=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Desired.Dot((T-Elbow).GetSafeNormal()),-1.,1.)));
  if(Bend>30&&Radius>.0001){const FVector Perp=FVector::VectorPlaneProject(Desired,Axis);const double M=Perp.Size();if(M>.0001){const double Min=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((Desired.Dot(T-Center)+Radius*M)/L2,-1.,1.)));const double Target=FMath::Max(32.,Min+.05);const double V=FMath::Clamp((Desired.Dot(T-Center)-L2*FMath::Cos(FMath::DegreesToRadians(Target)))/(Radius*M),-1.,1.);const FVector Dir=Perp/M,Tangent=FVector::VectorPlaneProject(Pole,Dir).GetSafeNormal();if(!Tangent.IsNearlyZero()){const FVector NewPole=Dir*V+Tangent*FMath::Sqrt(1-V*V);const FQuat Adjust=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(Pole,NewPole),FMath::SmoothStep(30.,35.,Bend));Elbow=Center+Adjust.RotateVector(Pole)*Radius;}}}
  if(StockContact&&ArmIndex==1&&Radius>.0001)
  {
   // ContactCarry is the final arm solve. Its old down-pole could undo the
   // earlier body's clearance and tuck the firing elbow inside the rib cage.
   // Search the SAME fixed-length circle outside the shoulder's side plane.
   // Hands, rifle, shoulder root and source bind are not moved.
   const FQuat ChestFrame=(P[Chest].GetRotation()*Bind[Chest].GetRotation().Inverse()).GetNormalized();
   const FVector Out=ChestFrame.RotateVector(Right).GetSafeNormal();
   const FVector BasePole=(Elbow-Center).GetSafeNormal();
   auto Score=[&](double Angle)
   {
    const FVector E=Center+FQuat(Axis,Angle).RotateVector(BasePole)*Radius;
    const double Inside=FMath::Max(0.,-(E-S).Dot(Out));
    const double Wrist=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Desired.Dot((T-E).GetSafeNormal()),-1.,1.)));
    return (E-Elbow).SizeSquared()+1000000.*(Inside*Inside+FMath::Square(FMath::Max(0.,Wrist-35.)));
   };
   double BestAngle=0,Best=Score(0);
   for(int I=-72;I<=72;++I){const double Angle=PI*I/72.;const double Value=Score(Angle);if(Value<Best){Best=Value;BestAngle=Angle;}}
   for(double Step:{.02,.005,.001})for(double Sign:{-1.,1.}){const double Angle=BestAngle+Sign*Step;const double Value=Score(Angle);if(Value<Best){Best=Value;BestAngle=Angle;}}
   Elbow=Center+FQuat(Axis,BestAngle).RotateVector(BasePole)*Radius;
  }
  // The feasible elbow circle has two sides. During idle-to-gait blending a
  // source pole can cross that side boundary in one frame. Follow the nearest
  // continuous arc rather than teleporting the elbow across the arm.
  if(Radius>.0001)
  {
   FVector FinalPole=(Elbow-Center).GetSafeNormal();
   if(PoleValid[ArmIndex])
   {
    const FVector Prior=FVector::VectorPlaneProject(PreviousPole[ArmIndex],Axis).GetSafeNormal();
    if(!Prior.IsNearlyZero())
    {
     const float Angle=FMath::Acos(FMath::Clamp(Prior.Dot(FinalPole),-1.,1.));
     const float Limit=FMath::DegreesToRadians(720.f*Clock.DeltaTime);
     if(Angle>Limit&&Limit>0.f)
     {
      const FQuat Step=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(Prior,FinalPole),Limit/Angle);
      FinalPole=Step.RotateVector(Prior).GetSafeNormal();
      Elbow=Center+FinalPole*Radius;
     }
    }
   }
   PreviousPole[ArmIndex]=FinalPole;PoleValid[ArmIndex]=true;
  }
  const FQuat Forearm=(FQuat::FindBetweenNormals(ForearmDesired,(T-Elbow).GetSafeNormal())*Deform).GetNormalized();
  P[A.Upper].SetRotation((FQuat::FindBetweenNormals((OldE-S).GetSafeNormal(),(Elbow-S).GetSafeNormal())*P[A.Upper].GetRotation()).GetNormalized());
  for(int I:{A.Elbow,A.Helper})P[I]=FTransform((Forearm*Bind[I].GetRotation()).GetNormalized(),Elbow+Forearm.RotateVector(Bind[I].GetLocation()-Bind[A.Elbow].GetLocation()),Bind[I].GetScale3D());
  Metrics.Y=FMath::Max(Metrics.Y,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Desired.Dot((T-Elbow).GetSafeNormal()),-1.,1.))));
  Metrics.Z=FMath::Max(Metrics.Z,FMath::Abs((T-Elbow).Size()-L2));
 }
 for(int I:Contacts){const auto Expected=Mix(B[I].GetRelativeTransform(B[RightWrist]),C[I].GetRelativeTransform(C[RightWrist]),Clock.Weight);Metrics.X=FMath::Max(Metrics.X,P[RightWrist].TransformVector(P[I].GetRelativeTransform(P[RightWrist]).GetLocation()-Expected.GetLocation()).Size());}
 for(int I=0;I<P.Num();++I){const auto K=Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));if(K.GetInt()<0)continue;Output.Pose[K]=Parents[I]>=0?P[I].GetRelativeTransform(P[Parents[I]]):P[I];Output.Pose[K].NormalizeRotation();}
}
