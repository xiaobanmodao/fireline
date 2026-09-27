#include "RangeBodyPose.h"
#include "RangeStockContact.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"

namespace
{
 FTransform Move(const FTransform& T,const FQuat& Q,const FVector& From,const FVector& To)
 {return FTransform((Q*T.GetRotation()).GetNormalized(),To+Q.RotateVector(T.GetLocation()-From),T.GetScale3D());}
 bool ChildOf(int32 I,int32 Parent,const TArray<int32>& Parents)
 {for(;I>=0;I=Parents[I])if(I==Parent)return true;return false;}
}
void FRangeBodyPose::Initialize(USkeletalMeshComponent* Mesh,UAnimSequence* Hold,bool IsMP7)
{
 Audit=FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyPoseDiagnosisAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace"));StockContact=FParse::Param(FCommandLine::Get(),TEXT("FirelineLocomotionContactFixStudy"));Enabled=false;HasFit=false;HasElbowHistory=false;HasContactHistory=false;HasTransferHistory=false;PendingWeaponTransfer=false;WeaponTransferActive=false;WeaponTransferTime=0;FitKey.Reset();if(!Hold)return;MP7=IsMP7;
 ContinuousContact=IsMP7&&FParse::Param(FCommandLine::Get(),TEXT("FirelineContinuousContactStudy"));
 const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();Bind=Ref.GetRefBonePose();Parents.Reset();Names.Reset();
 for(int I=0;I<Bind.Num();++I){Parents.Add(Ref.GetParentIndex(I));Names.Add(Ref.GetBoneName(I));if(I>0)Bind[I]=Bind[I]*Bind[Parents[I]];}
 auto Id=[&](const TCHAR* N){return Ref.FindBoneIndex(N);};
 Torso=Id(TEXT("Torso"));Chest=Id(TEXT("Chest"));Neck=Id(TEXT("Neck"));Head=Id(TEXT("Head"));
 Forward=Mesh->GetRelativeRotation().UnrotateVector(FVector::ForwardVector);Right=Mesh->GetRelativeRotation().UnrotateVector(FVector::RightVector);
 const TCHAR* Prefix=MP7?TEXT("MP7"):TEXT("M4");
 auto GunId=[&](const TCHAR* N){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),Prefix,N));};
 Rear=GunId(TEXT("rearsight"));Front=GunId(TEXT("frontsight"));SightUp=GunId(TEXT("sightup"));
 for(int I=0;I<2;++I)
 {
  auto& A=Arm[I];A.Side=I==0?-1.f:1.f;const TCHAR* Side=I==0?TEXT("L"):TEXT("R");
  auto Bone=[&](const TCHAR* N){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),N,Side));};
  A.Clavicle=Bone(TEXT("Shoulder"));A.Upper=Bone(TEXT("UpperArm"));A.Lower=Bone(TEXT("LowerArm"));A.Forearm=Bone(TEXT("DJ_forearm"));A.Wrist=Bone(TEXT("DJ_wrist"));A.Middle=Bone(TEXT("DJ_middle_01"));
  if(A.Wrist<0||A.Middle<0||A.Clavicle<0||A.Upper<0||A.Lower<0||A.Forearm<0)return;
  A.HandBones.Reset();
  for(int J=0;J<Names.Num();++J)if(ChildOf(J,A.Lower,Parents)&&J!=A.Lower&&J!=A.Forearm)A.HandBones.Add(J);
 }
 // Sample the source hold, independently of whichever draw/reload was visible
 // when a weapon switch recreated the body animation instance.
 auto* Sample=NewObject<USkeletalMeshComponent>(Mesh->GetOwner());Sample->SetSkeletalMesh(Mesh->GetSkeletalMeshAsset());Sample->SetVisibility(false);Sample->SetHiddenInGame(true);Sample->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sample->RegisterComponent();Sample->PlayAnimation(Hold,false);Sample->SetPosition(0,false);Sample->TickAnimation(0,false);Sample->RefreshBoneTransforms();
 Neutral.Reset();for(FName N:Names)Neutral.Add(Sample->GetSocketTransform(N,RTS_Component));Sample->DestroyComponent();
 Enabled=Torso>=0&&Chest>=0&&Neck>=0&&Head>=0&&(Rear>=0&&Front>=0&&SightUp>=0);
}
void FRangeBodyPose::Apply(FPoseContext& Output) const
{
 if(!Enabled||Weight<.001f)return;
 const bool ContinueCorrection=ContinuousContact;
 const float PosePitch=Pitch*(1.f-.8f*Reload);
 // Mesh changes issue a zero-time pose refresh before the first visible tick.
 // It still needs a feasible initialization, not an immovable old elbow with
 // a new gun grip. Do not advance the authored transfer clock on that refresh.
 const float SolveDelta=(SourceBodyConstraints||ContinueCorrection)&&FrameDeltaSeconds<=UE_SMALL_NUMBER?1.f/60.f:FrameDeltaSeconds;
 const auto& Container=Output.Pose.GetBoneContainer();TArray<int32> Compact;Compact.Init(-1,Names.Num());
 TArray<FTransform> Original;Original.SetNum(Names.Num());
 FCSPose<FCompactPose> CS;CS.InitPose(Output.Pose);
 for(int I=0;I<Names.Num();++I)
 {
  const auto C=Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));Compact[I]=C.GetInt();Original[I]=C.GetInt()>=0?CS.GetComponentSpaceTransform(C):Bind[I];
 }
 TArray<FTransform> P=Original;
 auto RotateBranch=[&](int32 Root,const FQuat& Q){const FVector Pivot=P[Root].GetLocation();for(int I=0;I<P.Num();++I)if(ChildOf(I,Root,Parents))P[I]=Move(P[I],Q,Pivot,Pivot);};
 // Carry the upper body in its reference chest frame; slide/fall lean stays
 // below this joint instead of being added a second time to the gun arm rig.
 // The isolated coordinated candidate carries its retargeted spine through
 // this pass. Fade back to the legacy chest policy outside ground locomotion.
 const FQuat ChestTarget=FQuat::Slerp(Neutral[Chest].GetRotation(),P[Chest].GetRotation(),FMath::Max(GroundChestWeight,SourceBodyWeight)).GetNormalized();
 RotateBranch(Chest,(ChestTarget*P[Chest].GetRotation().Inverse()).GetNormalized());
 // Match measured source sagittal spine segments before aim and the shared
 // two-arm weapon solve. These are anatomical chain angles, not bone Euler values.
 if(SourcePostureWeight>0.f)
 {
  auto PitchChain=[&](int Root,int End,float Degrees)
  {
   const FVector Chain=P[End].GetLocation()-P[Root].GetLocation();
   const float Current=FMath::RadiansToDegrees(FMath::Atan2(Chain.Dot(Forward),Chain.Dot(Up)));
   RotateBranch(Root,FQuat(Right,FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Current,Degrees)*SourcePostureWeight)));
  };
  PitchChain(Torso,Chest,SourceSpineLow);PitchChain(Chest,Neck,SourceSpineHigh);
  // Match the complete pelvis-to-neck stance, including Ryan's longer basal
  // spine segment. Matching two unrelated subsegments alone diluted the lean.
  const int Pelvis=Parents[Torso];
  const FVector Base=P[Torso].GetLocation()-P[Pelvis].GetLocation(),Upper=P[Neck].GetLocation()-P[Torso].GetLocation();
  const float Target=FMath::Atan(FMath::Tan(FMath::DegreesToRadians(SourceBodyLean))*SourceLeanScale);
  const FVector Whole=Base+Upper;const float Initial=FMath::Atan2(Whole.Dot(Forward),Whole.Dot(Up));
  const float Goal=FMath::Lerp(Initial,Target,SourcePostureWeight);
  float Lo=-40.f,Hi=40.f;
  for(int I=0;I<18;++I){const float Mid=(Lo+Hi)*.5f;const FVector D=Base+FQuat(Right,FMath::DegreesToRadians(Mid)).RotateVector(Upper);if(FMath::Atan2(D.Dot(Forward),D.Dot(Up))<Goal)Lo=Mid;else Hi=Mid;}
  RotateBranch(Torso,FQuat(Right,FMath::DegreesToRadians((Lo+Hi)*.5f)));
 }
 // The isolated side-step study carries a bounded roll from its source
 // shoulder line through the actual torso joint. Arms are solved afterwards.
 if(FMath::Abs(StrafeRoll)>.001f)RotateBranch(Torso,FQuat(Forward,FMath::DegreesToRadians(-StrafeRoll)));
 if(MP7){
  const FVector ShoulderLine=Neutral[Arm[1].Clavicle].TransformPosition(Bind[Arm[1].Clavicle].InverseTransformPosition(Bind[Arm[1].Upper].GetLocation()))-Neutral[Arm[0].Clavicle].TransformPosition(Bind[Arm[0].Clavicle].InverseTransformPosition(Bind[Arm[0].Upper].GetLocation()));
  RotateBranch(Chest,FQuat(Up,FMath::Atan2(ShoulderLine.Dot(Forward),ShoulderLine.Dot(Right))*Reload));
 }
 const FQuat PitchQ(Right,FMath::DegreesToRadians(-PosePitch));
 const FQuat YawQ(Up,FMath::DegreesToRadians(Yaw));
 // Increase torso participation toward vertical aim instead of asking the
 // shoulder and wrist alone to absorb the full remaining angle.
 const float Extreme=FMath::Clamp((FMath::Abs(PosePitch)-45.f)/44.f,0.f,1.f);
 const float SpinePitch=PosePitch*(PosePitch<0?.65f:(.35f+.2f*Extreme*Extreme*(3.f-2.f*Extreme)));
 // Distribute aim over the torso and chest without moving their attachment roots.
 RotateBranch(Torso,FQuat(Up,FMath::DegreesToRadians(Yaw*.25f))*FQuat(Right,FMath::DegreesToRadians(-SpinePitch*.5142857f)));
 RotateBranch(Chest,FQuat(Up,FMath::DegreesToRadians(Yaw*.25f))*FQuat(Right,FMath::DegreesToRadians(-SpinePitch*.4857143f)));
 // Head follows the aim direction; keep its original position in the neck chain.
 const FQuat HeadDeform=(P[Head].GetRotation()*Bind[Head].GetRotation().Inverse()).GetNormalized();
 RotateBranch(Neck,(YawQ*PitchQ*HeadDeform.Inverse()).GetNormalized());
 // The clavicles protract as the arms reach forward. Rotate their real
 // joints; never translate the humerus away from its attachment.
 const FVector ShoulderAxis=(FQuat(Up,FMath::DegreesToRadians(Yaw*.5f))*FQuat(Right,FMath::DegreesToRadians(-SpinePitch))).RotateVector(Up);
 for(const auto& A:Arm)RotateBranch(A.Clavicle,FQuat(ShoulderAxis,FMath::DegreesToRadians(-A.Side*(1.f-SourceBodyWeight)*FMath::Lerp(MP7?FMath::Lerp(15.f,35.f,Reload):15.f,8.f,SprintCarry))));
 FVector Shoulder[2];
 for(int I=0;I<2;++I)
 {
  const auto& A=Arm[I];
  // Restore the original fixed clavicle -> upper-arm attachment, no translation cheat.
  Shoulder[I]=P[A.Clavicle].TransformPosition(Bind[A.Clavicle].InverseTransformPosition(Bind[A.Upper].GetLocation()));
 }
 // Express incoming action motion relative to its neutral chest. This preserves
 // magazine/charging contacts while giving it a full-body presentation frame.
 auto Source=[&](int I)
 {
  FTransform Action=Original[I].GetRelativeTransform(Original[Chest])*Neutral[Chest];
  if(SourceBodyWeight>0.f)
  {
   // Retargeted shoulders are independent chains. Restore both gripping hands
   // and weapon in ONE native frame, before solving either arm.
   const FTransform Anchor=Original[Arm[1].Wrist].GetRelativeTransform(Original[Chest])*Neutral[Chest];
   const FTransform Grip=Neutral[I].GetRelativeTransform(Neutral[Arm[1].Wrist])*Anchor;
   Action.Blend(Action,Grip,SourceBodyWeight);
  }
  return Action;
 };

 {
  const FVector NRear=Neutral[Rear].GetLocation(),NFront=Neutral[Front].GetLocation();
  const FQuat Frame=FRotationMatrix::MakeFromXZ(NFront-NRear,Neutral[SightUp].GetLocation()-NRear).ToQuat();
  const FQuat CarryQ=FQuat::Slerp(FQuat::Identity,CarryRotation,CarryWeight).GetNormalized();
  const FQuat ReadyTarget=FRotationMatrix::MakeFromXZ(YawQ.RotateVector(PitchQ.RotateVector(Forward)),YawQ.RotateVector(PitchQ.RotateVector(Up))).ToQuat();
  // Authored chest-carry target: muzzle diagonally left/up, receiver near the
  // chest. Native grip/finger transforms stay rigid and both arms solve together.
  const FVector SprintForward=(Forward*.35f-Right*.72f+Up*.60f).GetSafeNormal();
  const FQuat SprintTarget=FRotationMatrix::MakeFromXZ(YawQ.RotateVector(SprintForward),YawQ.RotateVector(Up)).ToQuat();
  const FQuat Target=CarryQ*FQuat::Slerp(ReadyTarget,SprintTarget,SprintCarry).GetNormalized();
  const FVector ActionRear=Source(Rear).GetLocation();
  const FQuat ActionFrame=FRotationMatrix::MakeFromXZ(Source(Front).GetLocation()-ActionRear,Source(SightUp).GetLocation()-ActionRear).ToQuat();
  const FQuat ActionDelta=(Frame.Inverse()*ActionFrame).GetNormalized();
  // Viewmodel draw/reload tilts are deliberately exaggerated for the camera.
  // Retain their timing and internal hand/mechanical motion, but present the
  // whole assembly with a smaller, full-body rotation and travel envelope.
  const FQuat BodyAction=FQuat::Slerp(FQuat::Identity,ActionDelta,.15f).GetNormalized();
  const FQuat Q=(Target*BodyAction*ActionFrame.Inverse()).GetNormalized();
  // Raised ADS carry. Precise cheek/stock contact needs a dedicated art pose.
  const FVector HeadPos=P[Head].GetLocation();
  const FVector Offset=Forward*FMath::Lerp(32.f,28.f,Aim)+Right*FMath::Lerp(18.f,15.f,Aim)+Up*FMath::Lerp(-7.f,-1.f,Aim);
  const FVector ReadyRear=HeadPos+YawQ.RotateVector(PitchQ.RotateVector(Offset));
  const FVector SprintRear=HeadPos+YawQ.RotateVector(Forward*27.f+Right*18.f-Up*29.f);
  const FVector RearTarget=FMath::Lerp(ReadyRear,SprintRear,SprintCarry)+CarryTranslation*CarryWeight+(Target*Frame.Inverse()).RotateVector(ActionRear-NRear)*.5f;
  for(const auto& A:Arm)for(int B:A.HandBones)P[B]=Move(Source(B),Q,ActionRear,RearTarget);
 }
 // Preserve contact as one assembly if a drawn/reloading gun lies outside reach.
 // No stretching, moving the shoulder root, or individually detaching a gripping hand.
 float UpperLength[2],LowerLength[2];
 for(int I=0;I<2;++I)
 {
  const auto& A=Arm[I];UpperLength[I]=(Bind[A.Lower].GetLocation()-Bind[A.Upper].GetLocation()).Size();LowerLength[I]=(Bind[A.Wrist].GetLocation()-Bind[A.Lower].GetLocation()).Size();
 }
 auto ReachProjection=[&](const FVector& W0,const FVector& W1,const FVector& Initial)
 {
  const FVector Wrists[]={W0,W1};FVector Shift=Initial;
  for(int Pass=0;Pass<8;++Pass)for(int I=0;I<2;++I)
  {
   const FVector D=Wrists[I]+Shift-Shoulder[I];const double Max=UpperLength[I]+LowerLength[I]-1.;
   if(D.Size()>Max)Shift+=D.GetSafeNormal()*(Max-D.Size());
  }
  return Shift;
 };
 for(int Pass=0;Pass<8;++Pass)for(int I=0;I<2;++I)
 {
  const FVector Reach=P[Arm[I].Wrist].GetLocation()-Shoulder[I];const float Max=UpperLength[I]+LowerLength[I]-1.f;
  if(Reach.Size()>Max)
  {
   const FVector Shift=Reach.GetSafeNormal()*(Max-Reach.Size());
   for(int J=0;J<2;++J)for(int B:Arm[J].HandBones)P[B].AddToTranslation(Shift);
  }
 }
 const FQuat TorsoFrame=(P[Chest].GetRotation()*Bind[Chest].GetRotation().Inverse()).GetNormalized();
 auto BodyPenetration=[&](const FVector& E){
  const FVector D=TorsoFrame.UnrotateVector(E-P[Chest].GetLocation());
  const double Radius=FMath::Sqrt(FMath::Square(D.Dot(Forward)/20.)+FMath::Square(D.Dot(Right)/27.));
  return FMath::Max(0.,1.-Radius)*20.;
 };
 auto BehindBody=[&](const FVector& E){return FMath::Max(0.,-TorsoFrame.UnrotateVector(E-P[Chest].GetLocation()).Dot(Forward)-2.);};
 auto FindElbow=[&](int SideIndex,const FVector& Wrist){
  const auto& A=Arm[SideIndex];
  const FVector Axis=(Wrist-Shoulder[SideIndex]).GetSafeNormal();
  const double Distance=FVector::Distance(Wrist,Shoulder[SideIndex]);
  const double Along=(UpperLength[SideIndex]*UpperLength[SideIndex]-LowerLength[SideIndex]*LowerLength[SideIndex]+Distance*Distance)/(2*FMath::Max(Distance,.01));
  const FVector Center=Shoulder[SideIndex]+Axis*Along;
  const double Radius=FMath::Sqrt(FMath::Max(0.,UpperLength[SideIndex]*UpperLength[SideIndex]-Along*Along));
  const FVector Palm=(P[A.Middle].GetLocation()-P[A.Wrist].GetLocation()).GetSafeNormal();
  const FVector Preferred=FMath::Lerp(Shoulder[SideIndex]+TorsoFrame.RotateVector(Forward*8.f+Right*(A.Side*30.f)-Up*40.f),P[A.Lower].GetLocation(),SourceBodyWeight);
  const FVector DownPole=FVector::VectorPlaneProject(Preferred-Center,Axis).GetSafeNormal();
  const FVector HandPole=FVector::VectorPlaneProject(Wrist-Palm*LowerLength[SideIndex]-Center,Axis).GetSafeNormal();
  if(CoherentGroundFit&&Radius>.001)
  {
   // The elbow lies on the fixed-length arm's intersection circle. Pick the
   // point closest to the preferred down/out pole INSIDE the wrist cone,
   // rather than averaging two poles and letting the wrist absorb the error.
   // dot((W-E)/lowerLength, palm) >= cos(35 degrees) defines a circle arc.
   const double PalmAcross=FVector::VectorPlaneProject(Palm,Axis).Size();
   if(PalmAcross>.0001)
   {
    const double Threshold=(LowerLength[SideIndex]*FMath::Cos(FMath::DegreesToRadians(35.))-(Wrist-Center).Dot(Palm))/(Radius*PalmAcross);
    const double Limit=FMath::Acos(FMath::Clamp(Threshold,-1.,1.));
    const double Desired=FMath::Atan2(Axis.Dot(HandPole.Cross(DownPole)),HandPole.Dot(DownPole));
    // If the cone is unreachable, Limit=0 is the least-bent wrist. The common
    // assembly fit then moves both hands/weapon together to restore reach.
    double Chosen=FMath::Clamp(Desired,-Limit,Limit);
    if(SourceBodyConstraints)
    {
     // Select a feasible elbow arc before moving the shared weapon frame.
     // Matching a donor pole alone can put the elbow through this wider torso.
     auto ArcCost=[&](double Angle)
     {
      const FVector E=Center+Radius*FQuat(Axis,Angle).RotateVector(HandPole);
      const FVector Local=TorsoFrame.UnrotateVector(E-P[Chest].GetLocation());
      const double Across=FMath::Max(0.,8.-A.Side*Local.Dot(Right));
      const double Above=FMath::Max(0.,(E-Shoulder[SideIndex]).Dot(Up)+2.);
      return (E-Preferred).SizeSquared()+10000.*(FMath::Square(BodyPenetration(E))+FMath::Square(BehindBody(E))+Across*Across+Above*Above);
     };
     double Best=ArcCost(Chosen);
     for(int I=0;I<=24;++I){const double CandidateAngle=-Limit+2*Limit*I/24.;const double Cost=ArcCost(CandidateAngle);if(Cost<Best){Best=Cost;Chosen=CandidateAngle;}}
     for(double Step:{.04,.01,.0025})for(double Sign:{-1.,1.}){const double CandidateAngle=FMath::Clamp(Chosen+Sign*Step,-Limit,Limit);const double Cost=ArcCost(CandidateAngle);if(Cost<Best){Best=Cost;Chosen=CandidateAngle;}}
    }
    if(StableCarryElbows&&HasElbowHistory)
    {
     // Stay on the same feasible arc as the wrist cone opens. The closest
     // down-pole solution can otherwise jump between its two endpoints while
     // the preceding elbow still satisfies every length/wrist constraint.
     const FVector Previous=P[Chest].TransformPosition(PreviousElbowChest[SideIndex]);
     const FVector PreviousPole=FVector::VectorPlaneProject(Previous-Center,Axis).GetSafeNormal();
     if(!PreviousPole.IsNearlyZero())
     {
      const double RawPrior=FMath::Atan2(Axis.Dot(HandPole.Cross(PreviousPole)),HandPole.Dot(PreviousPole));
      const double Prior=SourceBodyConstraints?RawPrior:FMath::Clamp(RawPrior,-Limit,Limit);
      const double Travel=FMath::DegreesToRadians(720.*FMath::Clamp(double(SolveDelta),0.,.1));
      // Do not project history onto the opposite end of a shrinking wrist
      // cone: that projection itself can jump across the whole arm circle.
      // Preserve the continuous pole; the shared fit absorbs cone error.
      Chosen=SourceBodyConstraints?Prior+FMath::Clamp(FMath::FindDeltaAngleRadians(Prior,Chosen),-Travel,Travel):FMath::Clamp(Chosen,Prior-Travel,Prior+Travel);
     }
    }
    return Center+Radius*FQuat(Axis,Chosen).RotateVector(HandPole);
   }
  }
  return Center+Radius*(DownPole*(MP7?.6:.7)+HandPole*(MP7?.4:.3)).GetSafeNormal();
 };

 if(SourceBodyConstraints&&WeaponTransferActive&&HasTransferHistory&&HasElbowHistory)
 {
  const FVector Pivot=P[Arm[1].Wrist].GetLocation();
  if(PendingWeaponTransfer)
  {
   // The new gun already owns its native hand spacing. Register that whole
   // assembly against the last physical elbows before exposing its first pose.
   // A translation alone cannot absorb the two providers' palm-axis change.
   FVector OldWrist[2],OldElbow[2],Palm[2];
   for(int I=0;I<2;++I){OldWrist[I]=P[Chest].TransformPosition(PreviousWristChest[I]);OldElbow[I]=P[Chest].TransformPosition(PreviousElbowChest[I]);Palm[I]=(P[Arm[I].Middle].GetLocation()-P[Arm[I].Wrist].GetLocation()).GetSafeNormal();}
   auto Cost=[&](const FQuat& Rotation,const FVector& Shift)
   {
    const FVector W0=OldWrist[1]+Rotation.RotateVector(P[Arm[0].Wrist].GetLocation()-Pivot),W1=OldWrist[1];
    const FVector ActualShift=ReachProjection(W0,W1,Shift);
    double Value=.02*ActualShift.SizeSquared();
    for(int I=0;I<2;++I)
    {
     const auto& A=Arm[I];
     const FVector W=OldWrist[1]+ActualShift+Rotation.RotateVector(P[Arm[I].Wrist].GetLocation()-Pivot);
     // Evaluate the exact history-limited fixed-length elbow which will be
     // used after registration. Scoring the old elbow point is not equivalent
     // when the new shoulder/wrist changes the intersection circle.
     const FVector PalmDirection=Rotation.RotateVector(Palm[I]);
     const FTransform SavedWrist=P[A.Wrist],SavedMiddle=P[A.Middle];
     P[A.Wrist].SetTranslation(W);P[A.Middle].SetTranslation(W+PalmDirection);
     const FVector E=FindElbow(I,W);
     P[A.Wrist]=SavedWrist;P[A.Middle]=SavedMiddle;
     const double Bend=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((W-E).GetSafeNormal().Dot(PalmDirection),-1.,1.)));
     const double Reach=FMath::Max(0.,FVector::Distance(W,Shoulder[I])-(UpperLength[I]+LowerLength[I]-1.));
     const double Across=FMath::Max(0.,8.-A.Side*TorsoFrame.UnrotateVector(E-P[Chest].GetLocation()).Dot(Right));
     const double Above=FMath::Max(0.,TorsoFrame.UnrotateVector(E-Shoulder[I]).Dot(Up)+2.-FMath::Max(0.f,PosePitch)*.35);
     const double ElbowTravel=FMath::Max(0.,FVector::Distance(E,OldElbow[I])-3.);
     Value+=1000000.*(FMath::Square(FMath::Max(0.,Bend-35.))+Reach*Reach+Across*Across+Above*Above+FMath::Square(BodyPenetration(E))+FMath::Square(BehindBody(E))+ElbowTravel*ElbowTravel)+.2*(W-OldWrist[I]).SizeSquared()+80.*(E-OldElbow[I]).SizeSquared();
    }
    return Value;
   };
   FQuat Rotation=FQuat::Identity;FVector Shift=FVector::ZeroVector;double Best=Cost(Rotation,Shift);
   // Register the two-hand span first. A palm-only seed can leave the new
   // short grip pointing across the old long grip and collapse the elbow circle.
   const FQuat SpanRotation=FQuat::FindBetweenNormals((P[Arm[0].Wrist].GetLocation()-Pivot).GetSafeNormal(),(OldWrist[0]-OldWrist[1]).GetSafeNormal());
   const FVector SpanShift=((OldWrist[0]-OldWrist[1])-SpanRotation.RotateVector(P[Arm[0].Wrist].GetLocation()-Pivot))*.5;
   const double SpanCost=Cost(SpanRotation,SpanShift);
   if(SpanCost<Best){Rotation=SpanRotation;Shift=SpanShift;Best=SpanCost;}
   for(int I=0;I<2;++I)
   {
    const FQuat Candidate=FQuat::FindBetweenNormals(Palm[I],(OldWrist[I]-OldElbow[I]).GetSafeNormal());
    const double Value=Cost(Candidate,Shift);if(Value<Best){Rotation=Candidate;Best=Value;}
   }
   const FQuat ChestRotation=P[Chest].GetRotation();
   const FVector Axes[]={ChestRotation.RotateVector(FVector::XAxisVector),ChestRotation.RotateVector(FVector::YAxisVector),ChestRotation.RotateVector(FVector::ZAxisVector)};
   // The subsequent contact continuation allows only this sphere around the
   // previous actual right wrist. Solve registration inside the same domain.
   const double ContactTravel=360.*FMath::Clamp(double(SolveDelta),1./240.,1./20.);
   // Six-dimensional registration, keeping every hand and gun bone rigid.
   for(double Step:{16.,8.,4.,2.,1.,.5})for(int Pass=0;Pass<3;++Pass)for(const FVector& Axis:Axes)for(double Sign:{-1.,1.})
   {
    const FQuat Q=(FQuat(Axis,FMath::DegreesToRadians(Step*Sign))*Rotation).GetNormalized();const double RCost=Cost(Q,Shift);
    if(RCost<Best){Rotation=Q;Best=RCost;}
    const FVector T=(Shift+Axis*(Step*.5*Sign)).GetClampedToMaxSize(ContactTravel);const double TCost=Cost(Rotation,T);
    if(TCost<Best){Shift=T;Best=TCost;}
   }
   Shift=ReachProjection(OldWrist[1]+Rotation.RotateVector(P[Arm[0].Wrist].GetLocation()-Pivot),OldWrist[1],Shift);
   if(Audit)UE_LOG(LogTemp,Display,TEXT("SOURCE_TRANSFER cost=%.3f dt=%.6f shift=%s"),Best,FrameDeltaSeconds,*Shift.ToString());
   WeaponTransferRotationChest=(ChestRotation.Inverse()*Rotation*ChestRotation).GetNormalized();
   WeaponTransferShiftChest=P[Chest].InverseTransformVector(OldWrist[1]+Shift-Pivot);
   WeaponTransferTime=0;PendingWeaponTransfer=false;HasFit=false;FitKey.Reset();
  }
  const float T=FMath::Clamp(WeaponTransferTime/.18f,0.f,1.f),Blend=T*T*(3.f-2.f*T);
  const FQuat ChestRotation=P[Chest].GetRotation();
  const FQuat Q=FQuat::Slerp((ChestRotation*WeaponTransferRotationChest*ChestRotation.Inverse()).GetNormalized(),FQuat::Identity,Blend).GetNormalized();
  const FVector Target=Pivot+P[Chest].TransformVector(WeaponTransferShiftChest)*(1.f-Blend);
  for(const auto& A:Arm)for(int B:A.HandBones)P[B]=Move(P[B],Q,Pivot,Target);
  WeaponTransferTime+=FrameDeltaSeconds;if(WeaponTransferTime>=.18f)WeaponTransferActive=false;
 }

 {
  const float SprintFitWeight=SourceBodyConstraints?1.f:SprintCarryStudy?FMath::SmoothStep(0.f,.15f,FMath::Max(SprintCarry,PreviousSprintCarry)):0.f;
  const FQuat ChestDeform=(P[Chest].GetRotation()*Bind[Chest].GetRotation().Inverse()).GetNormalized();
  const FVector StockDelta=RangeStockContact::Pocket(Shoulder[1],ChestDeform,Forward,Right)-RangeStockContact::Butt(P,Rear,Front,SightUp,MP7);
  auto FitCost=[&](const FVector& RawShift){
   const FVector Shift=SourceBodyConstraints?ReachProjection(P[Arm[0].Wrist].GetLocation(),P[Arm[1].Wrist].GetLocation(),RawShift):RawShift;
   double Cost=.2*Shift.SizeSquared();
   if(StockContact)Cost+=20000.*StockWeight*(Shift-StockDelta).SizeSquared();
   // A moving chest exposes competing wrist/elbow fit minima. Penalize
   // frame-to-frame motion of the WHOLE contact assembly during this study;
   // otherwise the stateless coarse search can hop by tens of centimeters.
   // Scale by dt so the continuation is not tied to a 60 Hz renderer.
   if((CoherentGroundFit||ContinueCorrection)&&HasFit)
   {
    const double TimeScale=(1./60.)/FMath::Clamp(double(SolveDelta),1./240.,1./20.);
    Cost+=40.*TimeScale*TimeScale*(Shift-FitShift).SizeSquared();
   }
   for(int I=0;I<2;++I){
    const auto& A=Arm[I];const FVector W=P[A.Wrist].GetLocation()+Shift,E=FindElbow(I,W);
    const FVector Palm=(P[A.Middle].GetLocation()-P[A.Wrist].GetLocation()).GetSafeNormal();
    const double Angle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((W-E).GetSafeNormal().Dot(Palm),-1.,1.)));
    const double Reach=FMath::Max(0.,FVector::Distance(W,Shoulder[I])-(UpperLength[I]+LowerLength[I]-1.));
    const double Across=FMath::Max(0.,8.-A.Side*TorsoFrame.UnrotateVector(E-P[Chest].GetLocation()).Dot(Right));
    const double Above=FMath::Max(0.,TorsoFrame.UnrotateVector(E-Shoulder[I]).Dot(Up)+2.-FMath::Max(0.f,PosePitch)*.35);
    // A collapsing feasible wrist arc can teleport an elbow despite smooth
    // gun interpolation. Move the WHOLE contact assembly to keep the previous
    // chest-space bend reachable, rather than smoothing/stretching one joint.
    if(SprintFitWeight>0&&HasElbowHistory){
     const FVector Previous=P[Chest].TransformPosition(PreviousElbowChest[I]);
     const double Rate=(1./60.)/FMath::Clamp(double(SolveDelta),1./240.,1./20.);
     Cost+=80.*SprintFitWeight*Rate*Rate*(E-Previous).SizeSquared();
    }
    Cost+=(SourceBodyConstraints?1000000.:FMath::Lerp(24.,500.,double(SprintFitWeight)))*FMath::Square(FMath::Max(0.,Angle-35.))+1000000.*Reach*Reach+(SourceBodyConstraints?1000000.:2000.)*(Across*Across+Above*Above+FMath::Square(BodyPenetration(E))+FMath::Square(BehindBody(E)));
   }
   return Cost;
  };
  TArray<FVector> Key;for(int I=0;I<2;++I){Key.Add(Shoulder[I]);Key.Add(P[Arm[I].Wrist].GetLocation());Key.Add(P[Arm[I].Middle].GetLocation());}Key.Add(FVector(PosePitch,0,0));
  bool Changed=Key.Num()!=FitKey.Num();if(!Changed)for(int I=0;I<Key.Num();++I)Changed|=!Key[I].Equals(FitKey[I],.0001);
  if(Changed){
   // Continue from the last actual contact position, not last frame's offset
   // in a changing action frame. Bound the local search so it cannot teleport
   // between equally feasible elbow/stock minima.
   const bool ContinueContact=(SourceBodyConstraints||ContinueCorrection)&&HasContactHistory;
   // Continue only the correction for this path. Limiting the absolute
   // wrist position would delay the authored magazine/charging motion and
   // create new wrist errors during otherwise valid reload keyframes.
   const FVector PriorShift=ContinueCorrection&&HasContactHistory?P[Chest].TransformVector(PreviousFitChest):ContinueContact?P[Chest].TransformPosition(PreviousContactChest)-P[Arm[1].Wrist].GetLocation():FVector::ZeroVector;
   const double Travel=360.*FMath::Clamp(double(SolveDelta),1./240.,1./20.);
   auto Bounded=[&](const FVector& V){return ContinueContact?PriorShift+(V-PriorShift).GetClampedToMaxSize(Travel):V;};
   FVector Shift=ContinueContact?PriorShift:HasFit?FitShift:FVector::ZeroVector;double Best=FitCost(Shift);
   const FVector Zero=Bounded(FVector::ZeroVector);const double ZeroCost=FitCost(Zero);if(ZeroCost<Best){Shift=Zero;Best=ZeroCost;}
   if(!ContinueContact&&Best>180.)for(double X:{-20.,0.,20.,40.})for(double Y:{-12.,0.,12.})for(double Z:{-20.,0.,20.}){
    const FVector Candidate=Forward*X+Right*Y+Up*Z;const double Cost=FitCost(Candidate);
    if(Cost<Best){Shift=Candidate;Best=Cost;}
   }
   for(double Step:{8.,4.,2.,1.,.5,.25,.125,.0625})for(int Sweep=0;Sweep<2;++Sweep)for(FVector Axis:{Forward,Right,Up})for(double Sign:{-1.,1.}){
    const FVector Candidate=Bounded(Shift+Axis*(Step*Sign));const double Cost=FitCost(Candidate);
    if(Cost<Best){Shift=Candidate;Best=Cost;}
   }
   FitShift=SourceBodyConstraints?ReachProjection(P[Arm[0].Wrist].GetLocation(),P[Arm[1].Wrist].GetLocation(),Shift):Shift;FitKey=MoveTemp(Key);
  }
  HasFit=true;
  // A common shift keeps fingers, magazine, charging handle and receiver in
  // exactly the same contact frame. It is not a per-hand wrist correction.
  for(const auto& A:Arm)for(int B:A.HandBones)P[B].AddToTranslation(FitShift);
  for(int Pass=0;Pass<8;++Pass)for(int I=0;I<2;++I){
   const FVector Reach=P[Arm[I].Wrist].GetLocation()-Shoulder[I];const float Max=UpperLength[I]+LowerLength[I]-1.f;
   if(Reach.Size()>Max)for(const auto& A:Arm)for(int B:A.HandBones)P[B].AddToTranslation(Reach.GetSafeNormal()*(Max-Reach.Size()));
  }
 }
 for(int I=0;I<2;++I)
 {
  const auto& A=Arm[I];const FVector Wrist=P[A.Wrist].GetLocation();FVector Elbow,End;
  const FVector Pole=FindElbow(I,Wrist);
  AnimationCore::SolveTwoBoneIK(Shoulder[I],Original[A.Lower].GetLocation(),Wrist,Pole,Wrist,Elbow,End,UpperLength[I],LowerLength[I],false,1.,1.);
  // Upper arm uses a consistent bend plane; lower-arm twist follows the hand,
  // and the legacy/native forearm aliases share exactly one rigid deformation.
  const FQuat HandDeform=(P[A.Wrist].GetRotation()*Bind[A.Wrist].GetRotation().Inverse()).GetNormalized();
  const FVector RestDirection=(Bind[A.Wrist].GetLocation()-Bind[A.Lower].GetLocation()).GetSafeNormal();
  const FQuat F=(FQuat::FindBetweenNormals(HandDeform.RotateVector(RestDirection),(Wrist-Elbow).GetSafeNormal())*HandDeform).GetNormalized();
  const FVector RefUpper=(Neutral[A.Lower].GetLocation()-Neutral[A.Upper].GetLocation()).GetSafeNormal();
  const FVector RefLower=(Neutral[A.Wrist].GetLocation()-Neutral[A.Lower].GetLocation()).GetSafeNormal();
  const FQuat RefFrame=FRotationMatrix::MakeFromXZ(RefUpper,RefUpper.Cross(RefLower)).ToQuat();
  const FVector NewUpper=(Elbow-Shoulder[I]).GetSafeNormal(),NewLower=(Wrist-Elbow).GetSafeNormal();
  const FQuat NewFrame=FRotationMatrix::MakeFromXZ(NewUpper,NewUpper.Cross(NewLower)).ToQuat();
  const FQuat UpperRotation=(NewFrame*RefFrame.Inverse()*Neutral[A.Upper].GetRotation()).GetNormalized();
  P[A.Upper]=FTransform(UpperRotation,Shoulder[I],Bind[A.Upper].GetScale3D());
  P[A.Lower]=Move(Bind[A.Lower],F,Bind[A.Lower].GetLocation(),Elbow);
  P[A.Forearm]=Move(Bind[A.Forearm],F,Bind[A.Lower].GetLocation(),Elbow);
 }
 PreviousSprintCarry=SprintCarry;
 // A zero-time mesh refresh solves a display pose, not another animation frame.
 // Retain the previous physical history until an actual update has elapsed.
 const bool CommitHistory=!(SourceBodyConstraints||ContinueCorrection)||FrameDeltaSeconds>UE_SMALL_NUMBER||(ContinueCorrection?!HasContactHistory:!HasElbowHistory);
 if(SourceBodyConstraints&&CommitHistory){for(int I=0;I<2;++I)PreviousWristChest[I]=P[Chest].InverseTransformPosition(P[Arm[I].Wrist].GetLocation());HasTransferHistory=true;}
 if((SourceBodyConstraints||ContinueCorrection)&&CommitHistory){PreviousContactChest=P[Chest].InverseTransformPosition(P[Arm[1].Wrist].GetLocation());PreviousFitChest=P[Chest].InverseTransformVector(FitShift);HasContactHistory=true;}
 if(StableCarryElbows&&CommitHistory){for(int I=0;I<2;++I)PreviousElbowChest[I]=P[Chest].InverseTransformPosition(P[Arm[I].Lower].GetLocation());HasElbowHistory=true;}
 if(Audit){
  AuditMetrics=FVector(0,0,BIG_NUMBER);
  {
   for(const auto& A:Arm)for(int B:A.HandBones){
    const FVector Before=Original[Rear].InverseTransformPosition(Original[B].GetLocation());
    const FVector After=P[Rear].InverseTransformPosition(P[B].GetLocation());
    AuditMetrics.X=FMath::Max(AuditMetrics.X,P[Rear].TransformVector(After-Before).Size());
   }
  }
  for(const auto& A:Arm){AuditMetrics.Y=FMath::Max(AuditMetrics.Y,BodyPenetration(P[A.Lower].GetLocation()));AuditMetrics.Z=FMath::Min(AuditMetrics.Z,A.Side*TorsoFrame.UnrotateVector(P[A.Lower].GetLocation()-P[Chest].GetLocation()).Dot(Right));}
 }
 // Write in parent order from a complete CS snapshot; never mix stale local
 // transforms with already modified component-space children.
 for(int I=0;I<P.Num();++I)if(Compact[I]>=0)
 {
  FTransform Local=Parents[I]>=0?P[I].GetRelativeTransform(P[Parents[I]]):P[I];Local.NormalizeRotation();
  if(Weight<.999f){FTransform Blended;Blended.Blend(Output.Pose[FCompactPoseBoneIndex(Compact[I])],Local,Weight);Local=Blended;}
  Output.Pose[FCompactPoseBoneIndex(Compact[I])]=Local;
 }
}
