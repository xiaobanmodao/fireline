#include "RangeAnimInstance.h"
#include "RangeGroundTransition.h"
#include "RangeGroundInertial.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RangeGame.h"
#include "RangeBodyPose.h"
#include "RangeFootContact.h"
#include "RangeLocomotionRules.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BonePose.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
// Only native ground player state is transferable between weapon meshes.
// Local weapon actions and first-person playback must retain their own clocks.
struct FRangeGaitCarry
{
 FRangeGroundTransition Transition;
 TSharedPtr<FRangeGroundInertialSnapshot> Inertial;
 FRangeFootContact Feet;
 UBlendSpace* Asset=nullptr;
 FBlendFilter Filter;
 TArray<FBlendSampleData> Samples;
 FMarkerTickRecord Markers;
 int32 Triangle=-1;
 bool GroundInput=false;float SourcePostureWeight=0,SourceBodyWeight=0;
 bool ContactHistory=false,WasMP7=false,TransferHistory=false;FVector PriorContactChest=FVector::ZeroVector,PriorWristChest[2];
 FVector PriorElbowChest[2];bool ElbowHistory=false;
 float Time=0,Weight=0,Speed=0,Direction=0,LegacySpeed=0,Crouch=0,CarryWeight=0;
};
namespace
{
 // Read the samples actually ticked by the engine, including interpolation
 // and marker synchronization; do not reconstruct weights from desired input.
 struct FRangeTracedBlendSpace : FAnimNode_BlendSpacePlayer_Standalone
 {
  const TArray<FBlendSampleData>& Samples() const{return BlendSampleDataCache;}
  void Capture(FRangeGaitCarry& C) const
  {C.Asset=GetBlendSpace();C.Filter=BlendFilter;C.Samples=BlendSampleDataCache;C.Markers=MarkerTickRecord;C.Triangle=CachedTriangulationIndex;C.Time=InternalTimeAccumulator;}
  bool Restore(const FRangeGaitCarry& C)
  {
   if(!C.Asset||GetBlendSpace()!=C.Asset)return false;
   BlendFilter=C.Filter;BlendSampleDataCache=C.Samples;MarkerTickRecord=C.Markers;
   CachedTriangulationIndex=C.Triangle;InternalTimeAccumulator=C.Time;PreviousBlendSpace=C.Asset;
   return true;
  }
 };
 void CaptureRangePose(FPoseContext& Pose,TArray<FTransform>& Out)
 {
  Out.Reset();FCSPose<FCompactPose> CS;CS.InitPose(Pose.Pose);
  const auto& Bones=Pose.Pose.GetBoneContainer();
  for(FName Name:RangePoseTrace::Bones)
  {
   const int32 Index=Bones.GetReferenceSkeleton().FindBoneIndex(Name);
   if(Index==INDEX_NONE){Out.Add(FTransform::Identity);continue;}
   const auto Compact=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
   Out.Add(Compact.GetInt()>=0?CS.GetComponentSpaceTransform(Compact):FTransform::Identity);
  }
 }
 // Pass-through taps capture the actual evaluation, never evaluate a branch
 // twice, and perform no file I/O or world queries on animation workers.
 struct FRangePoseProbe : FAnimNode_Base
 {
  FPoseLink Source;bool Enabled=false,CaptureCarry=false,CaptureSide=false,CapturePlant=false;float Plant[2]={0,0},SpineLow=0,SpineHigh=0,BodyLean=0;TArray<FTransform> Pose;FVector CarryTranslation=FVector::ZeroVector;FQuat CarryRotation=FQuat::Identity;float SideRoll=0;
  virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override{Source.Initialize(C);}
  virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override{Source.CacheBones(C);}
  virtual void Update_AnyThread(const FAnimationUpdateContext& C) override{Source.Update(C);}
  virtual void Evaluate_AnyThread(FPoseContext& Output) override{Source.Evaluate(Output);if(Enabled)CaptureRangePose(Output,Pose);if(CapturePlant){BodyLean=Output.Curve.Get(TEXT("FL_BodyLean"));SpineLow=Output.Curve.Get(TEXT("FL_SpineLow"));SpineHigh=Output.Curve.Get(TEXT("FL_SpineHigh"));Plant[0]=Output.Curve.Get(TEXT("FL_Plant_L"));Plant[1]=Output.Curve.Get(TEXT("FL_Plant_R"));}if(CaptureSide)SideRoll=Output.Curve.Get(TEXT("FL_SideRoll"));if(CaptureCarry){CarryTranslation=FVector(Output.Curve.Get(TEXT("FL_CarryX")),Output.Curve.Get(TEXT("FL_CarryY")),Output.Curve.Get(TEXT("FL_CarryZ")));CarryRotation=FQuat(Output.Curve.Get(TEXT("FL_CarryQX")),Output.Curve.Get(TEXT("FL_CarryQY")),Output.Curve.Get(TEXT("FL_CarryQZ")),1.f+Output.Curve.Get(TEXT("FL_CarryQW"))).GetNormalized();}}
 };
}
struct FRangeAnimProxy : FAnimInstanceProxy
{
 FRangeBodyPose BodyPose;
 FRangeContactCarry ContactCarry;
 FRangeFootContact Feet;
 FRangePoseProbe GroundProbe,MobilityProbe,LayerProbe,PostureProbe;
 FRangeGroundInertial GroundInertial;
 bool SourceBodyStudy=false;
 bool GroundInput=false;
 bool TraceEnabled=false;TArray<FTransform> FinalTrace;
 // The opt-in body owns its auxiliary bones; accepted view meshes bypass this pass.
 struct FElbowBinding
 {
  int32 Upper=INDEX_NONE,Lower=INDEX_NONE;
  bool bKnee=false;
  struct FSection {int32 Bone=INDEX_NONE;FTransform Bind;};
  TArray<FSection> Sections;
  FVector RestJoint;
  FQuat UpperBind,LowerBind;
  FVector RestAxis;
  float TwistCenter=-100.f;
 };
 TArray<FElbowBinding> Elbows;
 struct FShoulderBinding
 {
  int32 Clavicle=INDEX_NONE,Upper=INDEX_NONE;
  FQuat ClavicleBind,UpperBind;
  FVector RestAxis;
  TArray<FElbowBinding::FSection> Sections;
 };
 TArray<FShoulderBinding> Shoulders;
 virtual bool Evaluate(FPoseContext& Output) override
 {
  if(Elbows.IsEmpty()&&!BodyPose.Enabled&&!TraceEnabled)return false;
  EvaluateAnimationNode(Output);
  if(ArmedLocomotion){
   BodyPose.CarryTranslation=GroundProbe.CarryTranslation;BodyPose.CarryRotation=GroundProbe.CarryRotation;
   if(SprintCarryStudy){
    // Remove the rejected static low-carry offset from evaluated high-speed samples.
    // Preserve their cyclic motion and the common marker-synchronized gait phase.
    float HighSpeed=0;for(const auto& S:DirectionalMovement.Samples())if(S.Animation&&S.Animation->GetName().StartsWith(TEXT("A_Sprint_")))HighSpeed+=S.TotalWeight;
    BodyPose.CarryTranslation.Z+=6.f*HighSpeed;
    BodyPose.CarryRotation=(FQuat(-FVector::XAxisVector,FMath::DegreesToRadians(-8.f*HighSpeed))*BodyPose.CarryRotation).GetNormalized();
    // Expand the evaluated gait, not a second oscillator. Both arms and the
    // weapon remain one contact assembly in the same chest/foot phase.
    // Ordinary locomotion is a quiet supported hold, not a smaller sprint.
    // Only the actual sprint state enables the large cycle. Remove almost
    // all fore/aft pumping in the ordinary hold, in the body's forward frame.
    const float Run=BodyPose.SprintCarry;
    const FVector Along=BodyPose.Forward*FVector::DotProduct(BodyPose.CarryTranslation,BodyPose.Forward);
    BodyPose.CarryTranslation=(BodyPose.CarryTranslation-Along)*FMath::Lerp(.30f,1.8f,Run)+Along*FMath::Lerp(.05f,1.8f,Run);
    BodyPose.CarryRotation=FQuat::Slerp(FQuat::Identity,BodyPose.CarryRotation,FMath::Lerp(.25f,1.65f,Run)).GetNormalized();
   }
  }
  BodyPose.StrafeRoll=CMUSideStepStudy?GroundProbe.SideRoll*BodyPose.StrafeRollWeight:0.f;
  if(BodyPose.Enabled){BodyPose.SourceBodyLean=PostureProbe.BodyLean;BodyPose.SourceSpineLow=PostureProbe.SpineLow;BodyPose.SourceSpineHigh=PostureProbe.SpineHigh;BodyPose.Apply(Output);ContactCarry.Apply(Output);Feet.GaitWeight=0;for(const auto& S:DirectionalMovement.Samples())if(S.Animation&&(S.Animation->GetPathName().StartsWith(TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/ContactFix/"))||(S.Animation->GetPathName().StartsWith(TEXT("/Game/Fireline/LocomotionStudy/SourceBody/"))||S.Animation->GetPathName().StartsWith(TEXT("/Game/Fireline/LocomotionStudy/CombatGround/")))||S.Animation->GetPathName().StartsWith(TEXT("/Game/Fireline/LocomotionStudy/MatchedGround/"))||S.Animation->GetPathName().StartsWith(TEXT("/Game/Fireline/LocomotionStudy/AuthoredGround/"))))Feet.GaitWeight+=S.TotalWeight;Feet.GaitWeight=DirectionalWeight*(Feet.GaitWeight*(1-Transition.Weight)+Transition.Weight);Feet.AuthoredKneeWeight=(FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineMatchedGroundStudy")))?DirectionalWeight:Transition.Weight;Feet.KeepStationaryPlant=Transition.Weight>.05f;Feet.Plant[0]=GroundProbe.Plant[0];Feet.Plant[1]=GroundProbe.Plant[1];Feet.Apply(Output);if(TraceEnabled)CaptureRangePose(Output,FinalTrace);return true;}
  if(Elbows.IsEmpty()){if(TraceEnabled)CaptureRangePose(Output,FinalTrace);return true;}
  FCSPose<FCompactPose> CS;CS.InitPose(Output.Pose);
  const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
  for(const FShoulderBinding& S:Shoulders)
  {
   auto Compact=[&](int32 Index){return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));};
   const auto C=Compact(S.Clavicle),U=Compact(S.Upper);
   if(C.GetInt()<0||U.GetInt()<0)continue;
   const FTransform Clavicle=CS.GetComponentSpaceTransform(C),Upper=CS.GetComponentSpaceTransform(U);
   const FQuat CQ=(Clavicle.GetRotation()*S.ClavicleBind.Inverse()).GetNormalized();
   const FQuat UQ=(Upper.GetRotation()*S.UpperBind.Inverse()).GetNormalized();
   const FQuat Swing=FQuat::FindBetweenNormals(CQ.RotateVector(S.RestAxis),UQ.RotateVector(S.RestAxis));
   const FQuat Twist=((Swing*CQ).Inverse()*UQ).GetNormalized();
   const float Angle=FMath::UnwindRadians(2.f*FMath::Atan2(FVector(Twist.X,Twist.Y,Twist.Z).Dot(S.RestAxis),Twist.W));
   constexpr float SwingFraction[]={.55f,.85f,1.f},TwistFraction[]={.20f,.60f,1.f};
   for(int32 I=0;I<S.Sections.Num();++I)
   {
    const auto& Section=S.Sections[I];const auto H=Compact(Section.Bone);if(H.GetInt()<0)continue;
    const FQuat Q=(FQuat::Slerp(FQuat::Identity,Swing,SwingFraction[I])*CQ*FQuat(S.RestAxis,Angle*TwistFraction[I])).GetNormalized();
    const FTransform Target((Q*Section.Bind.GetRotation()).GetNormalized(),Upper.GetLocation(),Upper.GetScale3D());
    Output.Pose[H]=Target.GetRelativeTransform(Clavicle);
   }
  }
  for(const FElbowBinding& E:Elbows)
  {
   auto Compact=[&](int32 Index){return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));};
   const auto U=Compact(E.Upper),L=Compact(E.Lower);
   if(U.GetInt()<0||L.GetInt()<0)continue;
   const FTransform Upper=CS.GetComponentSpaceTransform(U),Lower=CS.GetComponentSpaceTransform(L);
   const FQuat UQ=(Upper.GetRotation()*E.UpperBind.Inverse()).GetNormalized();
   const FQuat LQ=(Lower.GetRotation()*E.LowerBind.Inverse()).GetNormalized();
   const FQuat Swing=FQuat::FindBetweenNormals(UQ.RotateVector(E.RestAxis),LQ.RotateVector(E.RestAxis));
   const FQuat End=(Swing*UQ).GetNormalized();
   const FQuat Twist=(End.Inverse()*LQ).GetNormalized();
   float Angle=2.f*FMath::Atan2(FVector(Twist.X,Twist.Y,Twist.Z).Dot(E.RestAxis),Twist.W);
   const float Center=FMath::DegreesToRadians(E.TwistCenter);
   Angle=Center+FMath::UnwindRadians(Angle-Center);
   const FVector UD=UQ.RotateVector(E.RestAxis),LD=LQ.RotateVector(E.RestAxis);
   const float Bend=FMath::Acos(FMath::Clamp(UD.Dot(LD),-1.,1.));
   const float BendFactor=FMath::Square(Bend/FMath::DegreesToRadians(120.f));
   const float Extent=E.bKnee?FMath::Min(19.f,5.f+13.f*BendFactor):FMath::Min(16.f,7.f+9.f*BendFactor);
   for(const auto& Section:E.Sections)
   {
    const auto H=Compact(Section.Bone);if(H.GetInt()<0)continue;
    const FVector Delta=Section.Bind.GetLocation()-E.RestJoint;
    const float Offset=Delta.Dot(E.RestAxis);const FVector Perp=Delta-E.RestAxis*Offset;
    FVector Point,Tangent;
    if(Offset<=-Extent){Point=Lower.GetLocation()+UD*Offset;Tangent=UD;}
    else if(Offset>=Extent){Point=Lower.GetLocation()+LD*Offset;Tangent=LD;}
    else
    {
     const float T=(Offset+Extent)/(2*Extent);
     Point=Lower.GetLocation()-UD*Extent*FMath::Square(1-T)+LD*Extent*T*T;
     if(E.bKnee)Point=FMath::Lerp(Lower.GetLocation()+(Offset<=0?UD:LD)*Offset,Point,.25f);
     Tangent=(UD*(1-T)+LD*T).GetSafeNormal();
    }
    float Fraction=FMath::Clamp((Offset+16.f)/(E.bKnee?36.f:42.5f),0.f,1.f);Fraction=Fraction*Fraction*(3-2*Fraction);
    const FQuat Q=(FQuat::FindBetweenNormals(UD,Tangent)*UQ*FQuat(E.RestAxis,Angle*Fraction)).GetNormalized();
    const FTransform Target((Q*Section.Bind.GetRotation()).GetNormalized(),Point+Q.RotateVector(Perp),Lower.GetScale3D());
    Output.Pose[H]=Target.GetRelativeTransform(Upper);
   }
  }
  return true;
 }
 FAnimNode_BlendSpacePlayer_Standalone Movement;
 FRangeTracedBlendSpace DirectionalMovement;
 FAnimNode_TwoWayBlend GroundMovement,AuthoredGround,TransitionMotion;
 FAnimNode_SequenceEvaluator_Standalone TransitionCurrent,TransitionPrevious;
 FRangeGroundTransition Transition;
 bool DirectionalEnabled=false,BodyCoordination=false,ArmedLocomotion=false,SprintCarryStudy=false,CMUSideStepStudy=false;
 float DirectionalWeight=0,LocalDirection=0,ActualSpeed=0;
 FAnimNode_LayeredBoneBlend MP7HoldBody,MP7ActionBody;
 FAnimNode_SequenceEvaluator_Standalone MP7ActionPose;
 FAnimNode_SequenceEvaluator_Standalone M4PreviousPose;
 FAnimNode_TwoWayBlend M4Motion;
 TArray<UAnimSequence*> M4Clips;
 bool M4=false;
 int32 M4Index=0,M4PreviousIndex=0;
 float M4Time=0,M4PreviousTime=0,M4Mix=1,M4Weight=0;
 bool MP7=false,EmptyReload=false;
 float MP7ActionWeight=0,MP7Time=0;
 int32 MP7Index=0;
 TArray<UAnimSequence*> MP7Clips;
 FAnimNode_SequenceEvaluator_Standalone KnifeCurrent,KnifePrevious;
 FAnimNode_TwoWayBlend KnifeMotion;
 FAnimNode_LayeredBoneBlend KnifeBody;
 float KnifeWeight=0,KnifeMix=1,KnifeT=0,KnifePreviousT=0;
 int32 KnifeIndex=0,KnifePreviousIndex=0;
 TArray<UAnimSequence*> Knives;
 FAnimNode_SequenceEvaluator_Standalone Hold;
 FAnimNode_SequenceEvaluator_Standalone HandOpen,HandFist,HandTest;
 FAnimNode_TwoWayBlend HandGrip,HandMotion;
 FAnimNode_LayeredBoneBlend HandBody;
 float HandAlpha=0,FistAlpha=0,TestAlpha=0,TestTime=0;
 FAnimNode_SequenceEvaluator_Standalone Aim;
 FAnimNode_SequenceEvaluator_Standalone Reload;
 FAnimNode_SequenceEvaluator_Standalone Slide,Rise,Fall,Land;
 FAnimNode_TwoWayBlend AirPose,AirBody,SlideBody,LandBody;
 float AirWeight=0,SlideWeight=0,LandWeight=0,LandTime=0,FallWeight=0;
 FAnimNode_LayeredBoneBlend AimBody;
 FAnimNode_LayeredBoneBlend UpperBody;
 float Speed=0,Crouch=0,Alpha=0,AimWeight=0,Time=2.35f;
 FRangeAnimProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
 {
  M4Motion.A.SetLinkNode(&M4PreviousPose);M4Motion.B.SetLinkNode(&MP7ActionPose);
  KnifeMotion.A.SetLinkNode(&KnifePrevious);KnifeMotion.B.SetLinkNode(&KnifeCurrent);
  KnifeBody.BasePose.SetLinkNode(&HandBody);KnifeBody.BlendPoses.SetNum(1);KnifeBody.BlendPoses[0].SetLinkNode(&KnifeMotion);KnifeBody.BlendWeights.Add(0);KnifeBody.LayerSetup.SetNum(1);
  FBranchFilter KnifeFilter;KnifeFilter.BoneName=TEXT("Torso");KnifeFilter.BlendDepth=0;KnifeBody.LayerSetup[0].BranchFilters.Add(KnifeFilter);
  HandGrip.A.SetLinkNode(&HandOpen);HandGrip.B.SetLinkNode(&HandFist);
  HandMotion.A.SetLinkNode(&HandGrip);HandMotion.B.SetLinkNode(&HandTest);
  MP7HoldBody.BasePose.SetLinkNode(&AimBody);MP7HoldBody.BlendPoses.SetNum(1);MP7HoldBody.BlendPoses[0].SetLinkNode(&Hold);MP7HoldBody.BlendWeights.Add(0);MP7HoldBody.LayerSetup.SetNum(1);
  MP7ActionBody.BasePose.SetLinkNode(&UpperBody);MP7ActionBody.BlendPoses.SetNum(1);MP7ActionBody.BlendPoses[0].SetLinkNode(&MP7ActionPose);MP7ActionBody.BlendWeights.Add(0);MP7ActionBody.LayerSetup.SetNum(1);
  FBranchFilter MP7Filter;MP7Filter.BoneName=TEXT("Torso");MP7Filter.BlendDepth=0;
  MP7HoldBody.LayerSetup[0].BranchFilters.Add(MP7Filter);MP7ActionBody.LayerSetup[0].BranchFilters.Add(MP7Filter);
  HandBody.BasePose.SetLinkNode(&MP7ActionBody);HandBody.BlendPoses.SetNum(1);HandBody.BlendPoses[0].SetLinkNode(&HandMotion);
  HandBody.BlendWeights.Add(0);HandBody.LayerSetup.SetNum(1);
  FBranchFilter HandFilter;HandFilter.BoneName=TEXT("Torso");HandFilter.BlendDepth=0;HandBody.LayerSetup[0].BranchFilters.Add(HandFilter);
  AirPose.A.SetLinkNode(&Rise);AirPose.B.SetLinkNode(&Fall);
  GroundMovement.A.SetLinkNode(&Movement);GroundMovement.B.SetLinkNode(&DirectionalMovement);
  TransitionMotion.A.SetLinkNode(&TransitionPrevious);TransitionMotion.B.SetLinkNode(&TransitionCurrent);
  PostureProbe.Source.SetLinkNode(&GroundMovement);PostureProbe.CapturePlant=true;
  AuthoredGround.A.SetLinkNode(&PostureProbe);AuthoredGround.B.SetLinkNode(&TransitionMotion);
  GroundInertial.Source.SetLinkNode(&AuthoredGround);
  GroundProbe.Source.SetLinkNode(FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"))?static_cast<FAnimNode_Base*>(&GroundInertial):static_cast<FAnimNode_Base*>(&AuthoredGround));
  AirBody.A.SetLinkNode(&GroundProbe);AirBody.B.SetLinkNode(&AirPose);
  SlideBody.A.SetLinkNode(&AirBody);SlideBody.B.SetLinkNode(&Slide);
  LandBody.A.SetLinkNode(&SlideBody);LandBody.B.SetLinkNode(&Land);
  MobilityProbe.Source.SetLinkNode(&LandBody);AimBody.BasePose.SetLinkNode(&MobilityProbe);
  LayerProbe.Source.SetLinkNode(&KnifeBody);
  AimBody.BlendPoses.SetNum(1);AimBody.BlendPoses[0].SetLinkNode(&Aim);
  AimBody.BlendWeights.Add(0);AimBody.LayerSetup.SetNum(1);
  FBranchFilter AimFilter;AimFilter.BoneName=TEXT("Torso");AimFilter.BlendDepth=0;
  AimBody.LayerSetup[0].BranchFilters.Add(AimFilter);
  UpperBody.BasePose.SetLinkNode(&AimBody);
  UpperBody.BlendPoses.SetNum(1);UpperBody.BlendPoses[0].SetLinkNode(&Reload);
  UpperBody.BlendWeights.Add(0);UpperBody.LayerSetup.SetNum(1);
  FBranchFilter Filter;Filter.BoneName=TEXT("Torso");Filter.BlendDepth=0;
  UpperBody.LayerSetup[0].BranchFilters.Add(Filter);
  UpperBody.bMeshSpaceRotationBlend=false;
 }
 virtual FAnimNode_Base* GetCustomRootNode() override{return TraceEnabled?static_cast<FAnimNode_Base*>(&LayerProbe):static_cast<FAnimNode_Base*>(&KnifeBody);}
 virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Out) override{Out.Add(&GroundInertial);Out.Add(&PostureProbe);Out.Add(&AuthoredGround);Out.Add(&TransitionMotion);Out.Add(&TransitionCurrent);Out.Add(&TransitionPrevious);Out.Add(&GroundProbe);Out.Add(&MobilityProbe);Out.Add(&LayerProbe);Out.Add(&GroundMovement);Out.Add(&DirectionalMovement);Out.Add(&M4PreviousPose);Out.Add(&M4Motion);Out.Add(&MP7HoldBody);Out.Add(&MP7ActionBody);Out.Add(&MP7ActionPose);Out.Add(&KnifeCurrent);Out.Add(&KnifePrevious);Out.Add(&KnifeMotion);Out.Add(&KnifeBody);Out.Add(&HandOpen);Out.Add(&HandFist);Out.Add(&HandTest);Out.Add(&HandGrip);Out.Add(&HandMotion);Out.Add(&HandBody);Out.Add(&Slide);Out.Add(&Rise);Out.Add(&Fall);Out.Add(&Land);Out.Add(&AirPose);Out.Add(&AirBody);Out.Add(&SlideBody);Out.Add(&LandBody);Out.Add(&Movement);Out.Add(&Hold);Out.Add(&Aim);Out.Add(&AimBody);Out.Add(&Reload);Out.Add(&UpperBody);}
 virtual void Initialize(UAnimInstance* Instance) override
 {
  BodyPose=FRangeBodyPose();ContactCarry=FRangeContactCarry();
  auto* Owner=CastChecked<URangeAnimInstance>(Instance);
  const auto* MeshAsset=Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
  const FString MeshPath=MeshAsset->GetPathName();
  const bool Original=MeshPath.StartsWith(TEXT("/Game/Fireline/DrazenOriginal/"));
  const bool Drazen=Original||MeshPath.StartsWith(TEXT("/Game/Fireline/Drazen/"));
  const auto* Character=Cast<ARangeCharacter>(Instance->TryGetPawnOwner());
  const bool bFirstPerson=Character && Instance->GetSkelMeshComponent()==Character->Arms;
  TraceEnabled=!bFirstPerson&&FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace"));
  GroundProbe.Enabled=MobilityProbe.Enabled=LayerProbe.Enabled=TraceEnabled;
  AirBody.A.SetLinkNode(TraceEnabled?static_cast<FAnimNode_Base*>(&GroundProbe):static_cast<FAnimNode_Base*>(&GroundMovement));
  AimBody.BasePose.SetLinkNode(TraceEnabled?static_cast<FAnimNode_Base*>(&MobilityProbe):static_cast<FAnimNode_Base*>(&LandBody));
  const TCHAR* Directory=Original?TEXT("DrazenOriginal"):Drazen?(bFirstPerson?TEXT("Drazen/View"):TEXT("Drazen")):(bFirstPerson?TEXT("FirstPerson/Hands"):TEXT("Hands"));
  auto AssetPath=[Directory](const TCHAR* Name){return FString::Printf(TEXT("/Game/Fireline/%s/%s.%s"),Directory,Name,Name);};
  if(Original)for(auto* Node:{&KnifeBody,&HandBody,&AimBody,&UpperBody})for(auto& Layer:Node->LayerSetup)for(auto& Filter:Layer.BranchFilters)Filter.BoneName=TEXT("Belly");
  Owner->MovementAsset=LoadObject<UBlendSpace>(nullptr,Original?*AssetPath(TEXT("BS_M4")):Drazen?TEXT("/Game/Fireline/Drazen/BS_M4.BS_M4"):TEXT("/Game/Fireline/Hands/BS_M4.BS_M4"));
  SourceBodyStudy=!bFirstPerson&&FParse::Param(FCommandLine::Get(),TEXT("FirelineSourceBodyStudy"));
  Transition.MatchUpperBody=SourceBodyStudy;
  DirectionalEnabled=!bFirstPerson&&!Drazen&&FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalLocomotion"));
  ArmedLocomotion=!bFirstPerson&&FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedLocomotionStudy"));
  SprintCarryStudy=ArmedLocomotion&&FParse::Param(FCommandLine::Get(),TEXT("FirelineSprintCarryStudy"));
  GroundProbe.CaptureCarry=ArmedLocomotion;
  if(ArmedLocomotion)AirBody.A.SetLinkNode(&GroundProbe);
  const bool KneeContinuity=ArmedLocomotion||FParse::Param(FCommandLine::Get(),TEXT("FirelineKneeContinuityStudy"));
  const bool LegDirection=KneeContinuity||FParse::Param(FCommandLine::Get(),TEXT("FirelineGaitContinuityStudy"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineLegDirectionStudy"));
  BodyCoordination=DirectionalEnabled&&(LegDirection||FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyCoordinationStudy")));
  if(DirectionalEnabled)
  {
   const bool ContactStudy=FParse::Param(FCommandLine::Get(),TEXT("FirelineContactPhaseStudy"));
   const bool StrideStudy=FParse::Param(FCommandLine::Get(),TEXT("FirelineStrideCoverageStudy"));
   const bool CMUSideStep=FParse::Param(FCommandLine::Get(),TEXT("FirelineCMUSideStepStudy"));
   CMUSideStepStudy=CMUSideStep;
   Owner->DirectionalAsset=LoadObject<UBlendSpace>(nullptr,SourceBodyStudy?TEXT("/Game/Fireline/LocomotionStudy/SourceBody/Ready/BS_Directional"):FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"))?TEXT("/Game/Fireline/LocomotionStudy/CombatGround/Ready/BS_Directional"):FParse::Param(FCommandLine::Get(),TEXT("FirelineMatchedGroundStudy"))?TEXT("/Game/Fireline/LocomotionStudy/MatchedGround/Ready/BS_Directional"):FParse::Param(FCommandLine::Get(),TEXT("FirelineAuthoredSideJogStudy"))?TEXT("/Game/Fireline/LocomotionStudy/AuthoredGround/Ready/BS_Directional"):FParse::Param(FCommandLine::Get(),TEXT("FirelineLocomotionContactFixStudy"))?TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/ContactFix/Ready/BS_Directional"):CMUSideStep?TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/Ready/BS_Directional"):ArmedLocomotion&&FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedReadyStudy"))?TEXT("/Game/Fireline/LocomotionStudy/ArmedReady/Ready/BS_Directional"):ArmedLocomotion?TEXT("/Game/Fireline/LocomotionStudy/ArmedLocomotion/Ready/BS_Directional"):KneeContinuity?TEXT("/Game/Fireline/LocomotionStudy/KneeContinuity/Ready/BS_Directional"):LegDirection?TEXT("/Game/Fireline/LocomotionStudy/LegDirection/Ready/BS_Directional"):BodyCoordination?TEXT("/Game/Fireline/LocomotionStudy/BodyCoordination/Ready/BS_Directional"):StrideStudy?TEXT("/Game/Fireline/LocomotionStudy/StrideCoverage/Ready/BS_Directional"):ContactStudy?TEXT("/Game/Fireline/LocomotionStudy/ContactPhase/BS_Directional"):TEXT("/Game/Fireline/LocomotionStudy/Ready/BS_Directional"));
   DirectionalEnabled=Owner->DirectionalAsset!=nullptr;
   DirectionalMovement.SetBlendSpace(Owner->DirectionalAsset);
  }
  Transition.Enabled=DirectionalEnabled&&FParse::Param(FCommandLine::Get(),TEXT("FirelineAuthoredGroundStudy"));
  const TCHAR* TransitionFolder=SourceBodyStudy?TEXT("SourceBody"):TEXT("AuthoredGround");
  if(Transition.Enabled)for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* D:{TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")})for(const TCHAR* S:{TEXT("Start"),TEXT("Stop"),TEXT("Pivot")})
  {auto* A=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/%s/Ready/A_%s_%s_%s"),TransitionFolder,G,D,S));check(A);Owner->GroundTransitionAssets.Add(A);Transition.Clips.Add(A);}
  if(Transition.Enabled)for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* D:{TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")})
  {auto* A=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/%s/Ready/A_Mirror_%s_%s_Stop"),TransitionFolder,G,D));check(A);Owner->GroundTransitionAssets.Add(A);Transition.Clips.Add(A);}
  TransitionCurrent.SetSequence(Transition.Enabled?Transition.Clips[0]:Owner->HoldAsset.Get());TransitionPrevious.SetSequence(Transition.Enabled?Transition.Clips[0]:Owner->HoldAsset.Get());
  GroundProbe.CaptureSide=DirectionalEnabled&&CMUSideStepStudy;
  if(GroundProbe.CaptureSide)AirBody.A.SetLinkNode(&GroundProbe);
  Owner->ReloadAsset=LoadObject<UAnimSequence>(nullptr,*AssetPath(TEXT("A_M4_Reload")));
  Owner->HoldAsset=LoadObject<UAnimSequence>(nullptr,*AssetPath(TEXT("A_M4_Idle")));
  Owner->AimAsset=LoadObject<UAnimSequence>(nullptr,*AssetPath(TEXT("A_M4_Aim")));
  MP7=MeshPath.StartsWith(TEXT("/Game/Fireline/MP7/"));
  Owner->MP7Assets.Reset();MP7Clips.Reset();
  if(MP7)
  {
   for(const TCHAR* Name:{TEXT("Idle"),TEXT("Draw"),TEXT("Inspect"),TEXT("Reload"),TEXT("EmptyReload")})
   {
    auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/MP7/A_MP7_%s.A_MP7_%s"),Name,Name));
    check(Clip);Owner->MP7Assets.Add(Clip);MP7Clips.Add(Clip);
   }
   Owner->HoldAsset=MP7Clips[0];Owner->AimAsset=MP7Clips[0];Owner->ReloadAsset=MP7Clips[3];
  }
  M4=!Drazen&&MeshPath.StartsWith(TEXT("/Game/Fireline/Hands/"));
  Owner->M4Assets.Reset();M4Clips.Reset();
  if(M4)
  {
   for(const TCHAR* Name:{TEXT("Idle"),TEXT("Draw"),TEXT("Remove"),TEXT("Reload"),TEXT("TacticalReload")})
   {
    auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/SycgffM4/A_M4_%s.A_M4_%s"),Name,Name));
    check(Clip);Owner->M4Assets.Add(Clip);M4Clips.Add(Clip);
   }
   Owner->HoldAsset=M4Clips[0];Owner->AimAsset=M4Clips[0];Owner->ReloadAsset=M4Clips[3];
   MP7ActionBody.BlendPoses[0].SetLinkNode(&M4Motion);
  }
  M4PreviousPose.SetSequence(Owner->HoldAsset);M4PreviousPose.SetExplicitTime(0);
  MP7ActionPose.SetSequence(MP7?MP7Clips[1]:Owner->HoldAsset.Get());MP7ActionPose.SetExplicitTime(0);
  MP7HoldBody.BlendWeights[0]=(MP7||M4)?1.f:0.f;
  if(MP7||M4)for(auto* Node:{&MP7HoldBody,&MP7ActionBody,&UpperBody,&AimBody})
  {
   auto& Filters=Node->LayerSetup[0].BranchFilters;Filters.Reset();
   for(const TCHAR* Side:{TEXT("Shoulder_L"),TEXT("Shoulder_R")}){FBranchFilter Filter;Filter.BoneName=Side;Filter.BlendDepth=0;Filters.Add(Filter);}
  }
  Owner->KnifeAssets.Reset();Knives.Reset();
  const bool Karambit=Drazen||MeshPath.StartsWith(TEXT("/Game/Fireline/Karambit/"))||MeshPath.StartsWith(TEXT("/Game/Fireline/LightBody/SK_LightBodyKnife."));
  Elbows.Reset();Shoulders.Reset();
  if(MeshPath.StartsWith(TEXT("/Game/Fireline/LightBody/")))
  {
   const FReferenceSkeleton& Ref=MeshAsset->GetRefSkeleton();
   TArray<FTransform> Bind=Ref.GetRefBonePose();
   for(int32 I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
   for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
   {
    auto Index=[&](const TCHAR* Name){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),Name,Side));};
    FElbowBinding E;E.TwistCenter=(FString(Side)==TEXT("L"))?100.f:-100.f;E.Upper=Index(TEXT("UpperArm"));E.Lower=Index(TEXT("LowerArm"));
    const int32 Hand=Index(TEXT("UpperHand"));
    if(E.Upper==INDEX_NONE||E.Lower==INDEX_NONE||Hand==INDEX_NONE)continue;
    FShoulderBinding S;S.Clavicle=Index(TEXT("Shoulder"));S.Upper=E.Upper;
    if(S.Clavicle!=INDEX_NONE)
    {
     S.ClavicleBind=Bind[S.Clavicle].GetRotation();S.UpperBind=Bind[S.Upper].GetRotation();
     S.RestAxis=(Bind[E.Lower].GetLocation()-Bind[E.Upper].GetLocation()).GetSafeNormal();
     for(int32 I=0;I<3;++I)
     {
      const int32 Bone=Ref.FindBoneIndex(*FString::Printf(TEXT("ShoulderSection%02d_%s"),I,Side));
      if(Bone!=INDEX_NONE)S.Sections.Add({Bone,Bind[Bone]});
     }
     if(S.Sections.Num()==3)Shoulders.Add(S);
    }
    E.UpperBind=Bind[E.Upper].GetRotation();E.LowerBind=Bind[E.Lower].GetRotation();
    E.RestJoint=Bind[E.Lower].GetLocation();
    E.RestAxis=(Bind[Hand].GetLocation()-E.RestJoint).GetSafeNormal();
    for(int32 I=0;I<10;++I)
    {
     const int32 Bone=Ref.FindBoneIndex(*FString::Printf(TEXT("ArmSection%02d_%s"),I,Side));
     if(Bone!=INDEX_NONE)E.Sections.Add({Bone,Bind[Bone]});
    }
    if(E.Sections.Num()==10)Elbows.Add(E);
    FElbowBinding K;K.bKnee=true;K.TwistCenter=0.f;K.Upper=Index(TEXT("UpperLeg"));K.Lower=Index(TEXT("LowerLeg"));
    const int32 Foot=Index(TEXT("Foot"));
    if(K.Upper!=INDEX_NONE&&K.Lower!=INDEX_NONE&&Foot!=INDEX_NONE)
    {
     K.UpperBind=Bind[K.Upper].GetRotation();K.LowerBind=Bind[K.Lower].GetRotation();
     K.RestJoint=Bind[K.Lower].GetLocation();K.RestAxis=(Bind[Foot].GetLocation()-K.RestJoint).GetSafeNormal();
     for(int32 I=0;I<7;++I)
     {
      const int32 Bone=Ref.FindBoneIndex(*FString::Printf(TEXT("KneeSection%02d_%s"),I,Side));
      if(Bone!=INDEX_NONE)K.Sections.Add({Bone,Bind[Bone]});
     }
     if(K.Sections.Num()==7)Elbows.Add(K);
    }
   }
  }
  for(const TCHAR* Name:{TEXT("Hold"),TEXT("Draw"),TEXT("Inspect"),TEXT("Slash")}){auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_%s_%s.A_%s_%s"),Drazen?Directory:Karambit?(bFirstPerson?TEXT("FirstPerson/Karambit"):TEXT("Karambit")):TEXT("M9"),Karambit?TEXT("Karambit"):TEXT("M9"),Name,Karambit?TEXT("Karambit"):TEXT("M9"),Name));Owner->KnifeAssets.Add(Clip);Knives.Add(Clip);}
  KnifeCurrent.SetSequence(Knives[0]);KnifePrevious.SetSequence(Knives[0]);KnifeCurrent.SetExplicitTime(0);KnifePrevious.SetExplicitTime(0);
  if(!Elbows.IsEmpty())
  {
   int32 Arms=0,Knees=0;for(const auto& Binding:Elbows){if(Binding.bKnee)Knees+=Binding.Sections.Num();else Arms+=Binding.Sections.Num();}
   UE_LOG(LogTemp,Display,TEXT("LIGHT_BODY_JOINTS arms=%d knees=%d shoulders=%d karambit=%s skeleton=%s"),Arms,Knees,Shoulders.Num()*3,Karambit?TEXT("true"):TEXT("false"),*MeshAsset->GetSkeleton()->GetPathName());
  }
  Owner->HandAssets.Reset();
  for(const TCHAR* Name:{TEXT("Open"),TEXT("Fist"),TEXT("Test")})Owner->HandAssets.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_Hands_%s.A_Hands_%s"),Directory,Name,Name)));
  HandOpen.SetSequence(Owner->HandAssets[0]);HandFist.SetSequence(Owner->HandAssets[1]);HandTest.SetSequence(Owner->HandAssets[2]);HandOpen.SetExplicitTime(0);HandFist.SetExplicitTime(0);HandTest.SetExplicitTime(0);
  Owner->MobilityAssets.Reset();
  for(const TCHAR* Name:{TEXT("Slide"),TEXT("JumpRise"),TEXT("JumpFall"),TEXT("Land")})
   Owner->MobilityAssets.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_M4_%s.A_M4_%s"),Directory,Name,Name)));
  Slide.SetSequence(Owner->MobilityAssets[0]);Rise.SetSequence(Owner->MobilityAssets[1]);Fall.SetSequence(Owner->MobilityAssets[2]);Land.SetSequence(Owner->MobilityAssets[3]);
  Slide.SetExplicitTime(0);Rise.SetExplicitTime(0);Fall.SetExplicitTime(0);Land.SetExplicitTime(0);
  Aim.SetSequence(Owner->AimAsset);Aim.SetExplicitTime(0);
  Hold.SetSequence(Owner->HoldAsset);
  // Static reference grip matches both ends of reload. Keep the view arms
  // independent of the full-body locomotion blend.
  Hold.SetExplicitTime(1.f/30.f);
  UpperBody.BasePose.SetLinkNode(bFirstPerson?static_cast<FAnimNode_Base*>(&Hold):static_cast<FAnimNode_Base*>(&MP7HoldBody));
  Movement.SetBlendSpace(Owner->MovementAsset);Reload.SetSequence(Owner->ReloadAsset);
  Reload.SetExplicitTime(2.35f);
  if(!bFirstPerson&&!Drazen&&(M4||MP7))BodyPose.Initialize(Instance->GetSkelMeshComponent(),Owner->HoldAsset.Get(),MP7);
  if(M4&&!bFirstPerson&&!SourceBodyStudy&&FParse::Param(FCommandLine::Get(),TEXT("FirelineContactCarryStudy")))
  {
   Owner->ContactAssets.Reset();TArray<UAnimSequence*> Clips;
   for(const TCHAR* Path:{TEXT("/Game/Fireline/LocomotionStudy/FullArmTransitions/Contact/A_Routine"),TEXT("/Game/Fireline/LocomotionStudy/FullArmSource/Contact/A_Walk_Fwd"),TEXT("/Game/Fireline/LocomotionStudy/FullArmSource/Contact/A_Jog_Fwd")})
   {auto* Clip=LoadObject<UAnimSequence>(nullptr,Path);Owner->ContactAssets.Add(Clip);Clips.Add(Clip);}
   ContactCarry.Initialize(Instance->GetSkelMeshComponent(),Clips,BodyPose.Neutral);
   ContactCarry.CompactIdleSupport=CMUSideStepStudy;
   UE_LOG(LogTemp,Display,TEXT("CONTACT_CARRY_RUNTIME enabled=%d native_clips=%d"),ContactCarry.Enabled,Clips.Num());
  }
  GroundProbe.CapturePlant=!bFirstPerson&&DirectionalEnabled&&FParse::Param(FCommandLine::Get(),TEXT("FirelineLocomotionContactFixStudy"));
  if(GroundProbe.CapturePlant){Feet.Initialize(Instance->GetSkelMeshComponent());AirBody.A.SetLinkNode(&GroundProbe);}
  FAnimInstanceProxy::Initialize(Instance);
 }
 virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,Dt);
  if(auto* P=Cast<ARangeCharacter>(Instance->TryGetPawnOwner()))
  {
   const bool FP=Instance->GetSkelMeshComponent()==P->Arms;
   Speed=FMath::FInterpTo(Speed,FP?0:P->Locomotion.Speed/7,Dt,7);
   Feet.CorrectionOnly=FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"));
   Feet.MeshWorld=Instance->GetSkelMeshComponent()->GetComponentTransform();Feet.Speed=P->Locomotion.Speed;Feet.Grounded=P->Locomotion.Grounded&&!P->bIsCrouched&&!P->bSliding;Feet.DeltaTime=Dt;
   GroundInertial.ShareUpperBody=SourceBodyStudy;
   GroundInertial.Enabled=DirectionalEnabled&&FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"));
   if(GroundInertial.Enabled&&(FMath::Abs(FMath::FindDeltaAngleDegrees(LocalDirection,P->Locomotion.DirectionDegrees))>60.f||((ActualSpeed>260.f)!=(P->Locomotion.Speed>260.f))))GroundInertial.Request(.2f);
   ActualSpeed=P->Locomotion.Speed;LocalDirection=P->Locomotion.DirectionDegrees;
   const FVector Intent=P->GetActorRotation().UnrotateVector(P->Locomotion.InputAcceleration);
   const bool NewGroundInput=Intent.SizeSquared2D()>1.f;
   if(GroundInertial.Enabled&&NewGroundInput!=GroundInput)GroundInertial.Request(.2f);
   GroundInput=NewGroundInput;
   const auto PreviousGroundMode=Transition.Mode;
   const auto* Move=P->GetCharacterMovement();
   // UE AnimCharacterMovementLibrary stop predictor, including friction.
   const float Friction=FMath::Max(0.f,(Move->bUseSeparateBrakingFriction?Move->BrakingFriction:Move->GroundFriction)*Move->BrakingFrictionFactor);
   const float StopDistance=.5f*ActualSpeed*ActualSpeed/FMath::Max(1.f,Friction*ActualSpeed+Move->BrakingDecelerationWalking);
   if(Transition.Enabled){const FName B[]={TEXT("Foot_L"),TEXT("Foot_R"),TEXT("LowerLeg_L"),TEXT("LowerLeg_R")};for(int I=0;I<4;++I)Transition.LastJoints[I]=Instance->GetSkelMeshComponent()->GetSocketTransform(B[I],RTS_Component).GetLocation();}
   if(Transition.Enabled&&Transition.MatchUpperBody)
   {
    const FName B[]={TEXT("Torso"),TEXT("Chest"),TEXT("UpperArm_L"),TEXT("UpperArm_R")};
    auto* Mesh=Instance->GetSkelMeshComponent();
    for(int I=0;I<4;++I)Transition.LastUpper[I]=Mesh->GetSocketTransform(B[I],RTS_Component);
    Transition.UpperOrientationRadius[0]=FVector::Distance(Transition.LastUpper[0].GetLocation(),Transition.LastUpper[1].GetLocation());
    Transition.UpperOrientationRadius[1]=FVector::Distance(Transition.LastUpper[1].GetLocation(),Mesh->GetSocketTransform(TEXT("Neck"),RTS_Component).GetLocation());
   }
   Transition.Update(Dt,ActualSpeed,LocalDirection,FVector2D(Intent.X,Intent.Y),Feet.Grounded,StopDistance,P->GetCharacterMovement()->GetMaxSpeed()*FMath::Clamp(Intent.Size2D()/FMath::Max(1.f,P->GetCharacterMovement()->GetMaxAcceleration()),0.f,1.f));
   if(GroundInertial.Enabled&&Transition.Mode!=PreviousGroundMode)GroundInertial.Request(.2f);
   DirectionalWeight=FMath::FInterpConstantTo(DirectionalWeight,DirectionalEnabled&&!P->bIsCrouched?1.f:0.f,Dt,8.f);
   Crouch=FMath::FInterpConstantTo(Crouch,(!FP&&P->bIsCrouched)?100.f:0.f,Dt,500);
   AimWeight=FP?0.f:P->AimAlpha;
   BodyPose.Aim=P->AimAlpha;
   ContactCarry.Clock=P->ContactCarryClock;
   const FRotator Look=P->GetControlRotation().GetNormalized();
   const float DesiredPitch=FMath::FInterpTo(BodyPose.Pitch,P->bThirdPersonOrbit?0.f:FMath::Clamp(Look.Pitch,-89.9f,89.9f),Dt,14.f);
   BodyPose.Pitch+=FMath::Clamp(DesiredPitch-BodyPose.Pitch,-540.f*Dt,540.f*Dt);
   ContactCarry.Pitch=BodyPose.Pitch;
   BodyPose.Yaw=FMath::FInterpTo(BodyPose.Yaw,P->bThirdPersonOrbit?0.f:FMath::Clamp(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Look.Yaw),-65.f,65.f)*P->AimAlpha,Dt,14.f);
   AirWeight=FP?0.f:P->AirBlend;SlideWeight=FP?0.f:P->SlideBlend;
   FallWeight=FMath::FInterpTo(FallWeight,RangeLocomotion::FallBlend(P->Locomotion.VerticalSpeed),Dt,12);
   LandTime=FMath::Clamp(P->LandingTime,0.f,16.f/60.f);
   LandWeight=FP?0.f:FMath::Square(FMath::Sin(PI*LandTime/(16.f/60.f)))*P->LandingStrength*(Speed>15?.4f:1.f)*(1-SlideWeight);
   HandAlpha=FMath::FInterpConstantTo(HandAlpha,P->bEmptyHands?1.f:0.f,Dt,7.f);FistAlpha=P->HandFistAlpha;TestAlpha=FMath::FInterpConstantTo(TestAlpha,P->bHandTest?1.f:0.f,Dt,6.f);TestTime=P->HandTestTime;
   BodyPose.Weight=1.f-HandAlpha;
   KnifeWeight=P->bKnife?1.f:0.f;KnifeIndex=P->KnifeAction;KnifePreviousIndex=P->KnifePreviousAction;KnifeT=P->KnifeTime;KnifePreviousT=P->KnifePreviousTime;KnifeMix=P->KnifeBlend;
   Time=P->ReloadPosition;
   M4Index=P->M4Action;M4PreviousIndex=P->M4PreviousAction;M4Time=P->M4Time;M4PreviousTime=P->M4PreviousTime;M4Mix=P->M4Mix;M4Weight=P->M4Weight;
   MP7Index=P->MP7Action;MP7Time=P->MP7ActionTime;MP7ActionWeight=P->MP7ActionBlend;EmptyReload=P->bEmptyReload;
   Alpha=P->ReloadFireReturnLeft>0?FMath::FInterpConstantTo(Alpha,0.f,Dt,12.5f):FMath::FInterpTo(Alpha,(P->bReloading||P->bReloadPoseHeld)?1.f:0.f,Dt,25);
   const float StockTarget=(1-FMath::Max(Alpha,MP7?MP7ActionWeight:M4Weight))*(1-HandAlpha)*(1-KnifeWeight)*(1-P->SprintCarryAlpha)*(1-AirWeight)*(1-SlideWeight);
   BodyPose.StockWeight=FMath::FInterpConstantTo(BodyPose.StockWeight,StockTarget,Dt,8.f);ContactCarry.StockWeight=BodyPose.StockWeight;
   BodyPose.Reload=Alpha*FMath::SmoothStep(0.f,.15f,Time);
   BodyPose.CoherentGroundFit=(BodyCoordination||SourceBodyStudy)&&DirectionalEnabled;
   BodyPose.SourceBodyConstraints=SourceBodyStudy;
   BodyPose.StableCarryElbows=ArmedLocomotion||SourceBodyStudy;
   BodyPose.SprintCarryStudy=SprintCarryStudy;
   BodyPose.FrameDeltaSeconds=Dt;
   BodyPose.SprintCarry=SprintCarryStudy?P->SprintCarryAlpha*(1-P->AimAlpha)*(1-FMath::Max(Alpha,MP7?MP7ActionWeight:M4Weight))*(1-AirWeight)*(1-SlideWeight):0.f;
   const float DesiredCarryWeight=ArmedLocomotion&&!P->bFiringMovement?DirectionalWeight*(1-AirWeight)*(1-SlideWeight)*(1-LandWeight)*(1-AimWeight)*(1-FMath::Max(Alpha,MP7?MP7ActionWeight:M4Weight))*(1-HandAlpha)*(1-KnifeWeight):0.f;
   BodyPose.CarryWeight=P->bFiringMovement?0.f:FMath::FInterpConstantTo(BodyPose.CarryWeight,DesiredCarryWeight,Dt,10.f);
   // Draw/reload own the upper-body stance as well as the weapon. Keeping
   // sprint shoulders while the action lowers its hands creates an infeasible
   // elbow fit even when the weapon carry curves are already suppressed.
   const float ActionStance=ArmedLocomotion?BodyPose.CarryWeight:1.f;
   ContactCarry.PreserveGroundSpine=FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"));
   const float PostureTarget=(ContactCarry.PreserveGroundSpine||SourceBodyStudy)?DirectionalWeight*(1-AirWeight)*(1-SlideWeight)*(1-LandWeight)*(1-FMath::Max(Alpha,MP7?MP7ActionWeight:M4Weight))*(1-HandAlpha)*(1-KnifeWeight):0.f;
   BodyPose.SourceBodyWeight=SourceBodyStudy?FMath::FInterpConstantTo(BodyPose.SourceBodyWeight,PostureTarget,Dt,8.f):0.f;
   if(SourceBodyStudy)MP7HoldBody.BlendWeights[0]=1.f-BodyPose.SourceBodyWeight;
   BodyPose.SourceLeanScale=FMath::FInterpTo(BodyPose.SourceLeanScale,1.f+.4f*FMath::SmoothStep(180.f,450.f,ActualSpeed),Dt,8.f);
   BodyPose.SourcePostureWeight=FMath::FInterpConstantTo(BodyPose.SourcePostureWeight,SourceBodyStudy?0.f:PostureTarget,Dt,8.f);
   BodyPose.GroundChestWeight=BodyCoordination&&DirectionalEnabled?DirectionalWeight*(1-AirWeight)*(1-SlideWeight)*(1-LandWeight)*ActionStance:0.f;
   BodyPose.StrafeRollWeight=CMUSideStepStudy?DirectionalWeight*(1-AirWeight)*(1-SlideWeight)*(1-LandWeight)*(1-AimWeight)*(1-FMath::Max(Alpha,MP7?MP7ActionWeight:M4Weight))*(1-HandAlpha)*(1-KnifeWeight):0.f;
  }
 }
 virtual void Update(float Dt) override
 {
  MP7ActionBody.BlendWeights[0]=MP7?MP7ActionWeight:M4?M4Weight:0.f;
  if(M4){Reload.SetSequence(M4Clips[EmptyReload?3:4]);MP7ActionPose.SetSequence(M4Clips[M4Index]);MP7ActionPose.SetExplicitTime(M4Time);M4PreviousPose.SetSequence(M4Clips[M4PreviousIndex]);M4PreviousPose.SetExplicitTime(M4PreviousTime);M4Motion.Alpha=M4Mix*M4Mix*(3.f-2.f*M4Mix);}
  if(MP7){MP7ActionPose.SetSequence(MP7Clips[MP7Index]);MP7ActionPose.SetExplicitTime(MP7Time);Reload.SetSequence(MP7Clips[EmptyReload?4:3]);}
  KnifeCurrent.SetSequence(Knives[KnifeIndex]);KnifePrevious.SetSequence(Knives[KnifePreviousIndex]);KnifeCurrent.SetExplicitTime(KnifeT);KnifePrevious.SetExplicitTime(KnifePreviousT);KnifeMotion.Alpha=KnifeMix*KnifeMix*(3-2*KnifeMix);KnifeBody.BlendWeights[0]=KnifeWeight;
  HandBody.BlendWeights[0]=HandAlpha;HandGrip.Alpha=FistAlpha;HandMotion.Alpha=TestAlpha;HandTest.SetExplicitTime(TestTime);
  AirPose.Alpha=FallWeight;AirBody.Alpha=AirWeight;SlideBody.Alpha=SlideWeight;LandBody.Alpha=LandWeight;Land.SetExplicitTime(LandTime);
  Movement.SetPlayRate(FMath::Lerp(1.22f,.96f,Crouch/100.f));
  GroundMovement.Alpha=DirectionalWeight;
  AuthoredGround.Alpha=Transition.Weight;TransitionMotion.Alpha=FMath::SmoothStep(0.f,1.f,Transition.Mix);
  if(Transition.Enabled){TransitionCurrent.SetSequence(Transition.Clips[Transition.Clip]);TransitionCurrent.SetExplicitTime(Transition.Time);TransitionPrevious.SetSequence(Transition.Clips[Transition.PreviousClip]);TransitionPrevious.SetExplicitTime(Transition.PreviousTime);}
  if(DirectionalEnabled)
  {
   const bool Matched=FParse::Param(FCommandLine::Get(),TEXT("FirelineMatchedGroundStudy"));
   float BlendSpeed=ActualSpeed;
   // Locomotion remains in its moving state during an input-driven reversal.
   // Capsule speed can cross zero without briefly selecting the idle pose.
   if(GroundInertial.Enabled&&GroundInput)BlendSpeed=FMath::Max(180.f,BlendSpeed);
   // Authored stop owns the transition; do not also collapse its base gait
   // to an unrelated idle during the same blend-in. Switch under full cover.
   const bool StopBase=Matched&&Transition.Mode==FRangeGroundTransition::EMode::Stop&&Transition.Weight<.999f;
   if(StopBase)BlendSpeed=Transition.EntrySpeed;
   DirectionalMovement.SetPosition(FVector(LocalDirection,BlendSpeed,0));
   const float NormalRate=ArmedLocomotion?1.f:FMath::Clamp(ActualSpeed/450.f,1.f,1.2f);
   const float SideAngleWeight=FMath::Clamp((45.f-FMath::Abs(FMath::Abs(LocalDirection)-90.f))/22.5f,0.f,1.f);
   const float SideSpeedWeight=1.f-FMath::Clamp((ActualSpeed-180.f)/90.f,0.f,1.f);
   const float SideRate=FMath::Clamp(ActualSpeed/180.f,.4f,1.15f);
   const float PlayRate=CMUSideStepStudy?FMath::Lerp(NormalRate,SideRate,SideAngleWeight*SideSpeedWeight):NormalRate;
   DirectionalMovement.SetPlayRate(StopBase?0.f:GroundInertial.Enabled&&GroundInput&&ActualSpeed<180.f?ActualSpeed/180.f:Matched?NormalRate:PlayRate);
  }
  AimBody.BlendWeights[0]=AimWeight;
  Movement.SetPosition(FVector(Speed,Crouch,0));Reload.SetExplicitTime(Time);UpperBody.BlendWeights[0]=Alpha;
 }
};
FAnimInstanceProxy* URangeAnimInstance::CreateAnimInstanceProxy(){return new FRangeAnimProxy(this);}
void URangeAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}

FVector URangeAnimInstance::GetBodyPoseAuditMetrics(){return GetProxyOnGameThread<FRangeAnimProxy>().BodyPose.AuditMetrics;}
bool URangeAnimInstance::GetGroundCarry(FTransform& Out)
{
 // Reading the completed body proxy on the game thread waits for any worker.
 // Reuse its evaluated curves; never start a second viewmodel gait clock.
 const auto& P=GetProxyOnGameThread<FRangeAnimProxy>();
 if(P.ContactCarry.Enabled){Out=P.ContactCarry.Cycle;return true;}
 if(!P.ArmedLocomotion||!P.DirectionalEnabled)return false;
 Out=P.SprintCarryStudy?FTransform(P.BodyPose.CarryRotation,P.BodyPose.CarryTranslation):FTransform(P.GroundProbe.CarryRotation,P.GroundProbe.CarryTranslation);
 return true;
}
FRangeLocomotionTrace URangeAnimInstance::GetLocomotionTrace()
{
 const auto& P=GetProxyOnGameThread<FRangeAnimProxy>();FRangeLocomotionTrace T;
 T.FootPlant=FVector(P.Feet.Plant[0],P.Feet.Plant[1],0);T.FootLocked=FVector(P.Feet.Feet[0].Locked,P.Feet.Feet[1].Locked,0);
 T.ContactWeight=P.ContactCarry.Enabled?P.ContactCarry.Clock.Weight:0;T.ContactMetrics=P.ContactCarry.Metrics;
 T.Ground=P.GroundProbe.Pose;T.Mobility=P.MobilityProbe.Pose;T.Layered=P.LayerProbe.Pose;T.Final=P.FinalTrace;
 T.Weights=FVector(P.AirWeight,P.SlideWeight,P.LandWeight);T.DirectionalWeight=P.DirectionalWeight;
 if(P.TraceEnabled&&P.Transition.Enabled&&P.Transition.Weight>0) {FRangeBlendSampleTrace S;S.Clip=P.Transition.Clips[P.Transition.Clip]->GetPathName();S.Time=P.Transition.Time;S.Weight=P.Transition.Weight;T.Samples.Add(S);}
 if(P.TraceEnabled)for(const auto& S:P.DirectionalMovement.Samples())if(S.Animation&&S.TotalWeight>0.0001f)
 {
  FRangeBlendSampleTrace Sample;Sample.Clip=S.Animation->GetName();Sample.Weight=S.TotalWeight;
  Sample.Time=S.Time;Sample.PreviousTime=S.PreviousTime;
  Sample.PreviousMarker=S.MarkerTickRecord.PreviousMarker.MarkerIndex;Sample.NextMarker=S.MarkerTickRecord.NextMarker.MarkerIndex;
  T.Samples.Add(MoveTemp(Sample));
 }
 return T;
}

TSharedPtr<FRangeGaitCarry> URangeAnimInstance::CaptureGait()
{
 // Every directional candidate shares the same ground player across weapon meshes.
 // Restricting this transfer to older study flags resets CMU legs on each switch.
 const auto& P=GetProxyOnGameThread<FRangeAnimProxy>();if(!P.DirectionalEnabled)return nullptr;
 auto C=MakeShared<FRangeGaitCarry>();C->Transition=P.Transition;P.DirectionalMovement.Capture(*C);
 C->WasMP7=P.MP7;C->TransferHistory=P.BodyPose.HasTransferHistory;for(int I=0;I<2;++I)C->PriorWristChest[I]=P.BodyPose.PreviousWristChest[I];C->ContactHistory=P.BodyPose.HasContactHistory;C->PriorContactChest=P.BodyPose.PreviousContactChest;C->SourceBodyWeight=P.BodyPose.SourceBodyWeight;C->ElbowHistory=P.BodyPose.HasElbowHistory;for(int I=0;I<2;++I)C->PriorElbowChest[I]=P.BodyPose.PreviousElbowChest[I];C->GroundInput=P.GroundInput;C->SourcePostureWeight=P.BodyPose.SourcePostureWeight;C->Feet=P.Feet;C->Inertial=P.GroundInertial.Capture();
 C->Weight=P.DirectionalWeight;C->Speed=P.ActualSpeed;C->Direction=P.LocalDirection;C->LegacySpeed=P.Speed;C->Crouch=P.Crouch;C->CarryWeight=P.BodyPose.CarryWeight;
 return C;
}
void URangeAnimInstance::RestoreGait(const TSharedPtr<FRangeGaitCarry>& C)
{
 if(!C)return;auto& P=GetProxyOnGameThread<FRangeAnimProxy>();
 if(!P.DirectionalEnabled||!P.DirectionalMovement.Restore(*C))return;
 if(P.Feet.Enabled&&C->Feet.Enabled)
 {
  // Transfer stance history, never the old weapon mesh's bone indices/bind.
  P.Feet.PelvisDrop=C->Feet.PelvisDrop;
  for(int I=0;I<2;++I){auto& N=P.Feet.Feet[I];const auto& Old=C->Feet.Feet[I];N.Locked=Old.Locked;N.Releasing=Old.Releasing;N.Weight=Old.Weight;N.Anchor=Old.Anchor;}
 }
 if(P.Transition.Enabled){auto Clips=P.Transition.Clips;P.Transition=C->Transition;P.Transition.Clips=MoveTemp(Clips);P.AuthoredGround.Alpha=P.Transition.Weight;P.TransitionMotion.Alpha=FMath::SmoothStep(0.f,1.f,P.Transition.Mix);P.TransitionCurrent.SetSequence(P.Transition.Clips[P.Transition.Clip]);P.TransitionCurrent.SetExplicitTime(P.Transition.Time);P.TransitionPrevious.SetSequence(P.Transition.Clips[P.Transition.PreviousClip]);P.TransitionPrevious.SetExplicitTime(P.Transition.PreviousTime);}
 if(P.SourceBodyStudy){P.BodyPose.HasContactHistory=C->ContactHistory;P.BodyPose.PreviousContactChest=C->PriorContactChest;P.BodyPose.SourceBodyWeight=C->SourceBodyWeight;P.BodyPose.HasElbowHistory=C->ElbowHistory;P.BodyPose.HasTransferHistory=C->TransferHistory;for(int I=0;I<2;++I){P.BodyPose.PreviousElbowChest[I]=C->PriorElbowChest[I];P.BodyPose.PreviousWristChest[I]=C->PriorWristChest[I];}P.BodyPose.PendingWeaponTransfer=P.BodyPose.WeaponTransferActive=C->TransferHistory&&C->WasMP7!=P.MP7;}P.GroundInput=C->GroundInput;P.BodyPose.SourcePostureWeight=C->SourcePostureWeight;P.GroundInertial.Restore(C->Inertial);P.DirectionalWeight=C->Weight;P.ActualSpeed=C->Speed;P.LocalDirection=C->Direction;P.Speed=C->LegacySpeed;P.Crouch=C->Crouch;P.BodyPose.CarryWeight=C->CarryWeight;
 P.GroundMovement.Alpha=C->Weight;P.DirectionalMovement.SetPosition(FVector(C->Direction,C->Speed,0));P.Movement.SetPosition(FVector(P.Speed,P.Crouch,0));
 UE_LOG(LogTemp,Display,TEXT("GAIT_CONTINUITY_RESTORED phase=%.6f weight=%.5f speed=%.4f samples=%d"),C->Time,C->Weight,C->Speed,C->Samples.Num());
}
