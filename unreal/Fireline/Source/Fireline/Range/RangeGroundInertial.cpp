#include "RangeGroundInertial.h"
#include "RangeTorsoComponentBlend.h"

struct FRangeGroundInertialSnapshot
{
 struct FBone
 {
  FName Name, Parent;
  FQuat InputPrevious, InputOlder, OutputPrevious, OutputOlder;
  FQuat Offset = FQuat::Identity;
  FVector AngularVelocityOffset = FVector::ZeroVector;
 };
 TArray<FBone> Bones;
 float PreviousDelta = 0.f, PendingDuration = 0.f, BlendDuration = 0.f, Elapsed = 0.f;
 int32 HistoryFrames = 0;
 bool Active = false;
};

TSharedPtr<FRangeGroundInertialSnapshot> FRangeGroundInertial::Capture() const
{
 if (HistoryFrames == 0) return nullptr;
 auto Snapshot = MakeShared<FRangeGroundInertialSnapshot>();
 TArray<FName> SharedBones = {TEXT("Pelvis"), TEXT("UpperLeg_L"), TEXT("UpperLeg_R"),
  TEXT("LowerLeg_L"), TEXT("LowerLeg_R"), TEXT("Foot_L"), TEXT("Foot_R")};
 if(ShareUpperBody)SharedBones.Append({TEXT("Torso"),TEXT("Chest"),TEXT("Neck"),TEXT("Head"),TEXT("Shoulder_L"),TEXT("Shoulder_R"),TEXT("UpperArm_L"),TEXT("UpperArm_R"),TEXT("LowerArm_L"),TEXT("LowerArm_R")});
 for (FName Name : SharedBones)
 {
  const int32 Index = BoneNames.IndexOfByKey(Name);
  if (!InputPrevious.IsValidIndex(Index) || !OutputPrevious.IsValidIndex(Index)) continue;
  auto& Bone = Snapshot->Bones.AddDefaulted_GetRef();
  Bone.Name = Name; Bone.Parent = ParentNames[Index];
  Bone.InputPrevious = InputPrevious[Index].GetRotation();
  Bone.OutputPrevious = OutputPrevious[Index].GetRotation();
  Bone.InputOlder = InputOlder.IsValidIndex(Index) ? InputOlder[Index].GetRotation() : Bone.InputPrevious;
  Bone.OutputOlder = OutputOlder.IsValidIndex(Index) ? OutputOlder[Index].GetRotation() : Bone.OutputPrevious;
  if (Active && Offsets.IsValidIndex(Index) && VelocityOffsets.IsValidIndex(Index))
  {
   Bone.Offset = Offsets[Index].GetRotation();
   Bone.AngularVelocityOffset = VelocityOffsets[Index].AngularVelocity;
  }
 }
 if (Snapshot->Bones.IsEmpty()) return nullptr;
 Snapshot->PreviousDelta = PreviousDelta;
 Snapshot->PendingDuration = PendingDuration;
 Snapshot->BlendDuration = BlendDuration;
 Snapshot->Elapsed = Elapsed;
 Snapshot->HistoryFrames = HistoryFrames;
 Snapshot->Active = Active;
 return Snapshot;
}

void FRangeGroundInertial::Restore(const TSharedPtr<FRangeGroundInertialSnapshot>& Snapshot)
{
 Clear();
 PendingRestore = Snapshot;
}

void FRangeGroundInertial::Clear()
{
 PendingRestore.Reset(); BoneNames.Reset(); ParentNames.Reset();
 InputPrevious.Reset(); InputOlder.Reset();
 OutputPrevious.Reset(); OutputOlder.Reset();
 Offsets.Reset(); VelocityOffsets.Reset();
 AccumulatedDelta = PreviousDelta = PendingDuration = BlendDuration = Elapsed = 0.f;
 HistoryFrames = 0;
 Active = false;
}

void FRangeGroundInertial::Request(float Duration)
{
 if (!FMath::IsFinite(Duration) || Duration <= 0.f) { Clear(); return; }
 PendingDuration = PendingDuration > 0.f ? FMath::Min(PendingDuration, Duration) : Duration;
}

void FRangeGroundInertial::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
 FAnimNode_Base::Initialize_AnyThread(Context);
 auto RestoreAfterInitialize = MoveTemp(PendingRestore);
 Clear();
 PendingRestore = MoveTemp(RestoreAfterInitialize);
 Source.Initialize(Context);
}

void FRangeGroundInertial::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
 FAnimNode_Base::CacheBones_AnyThread(Context);
 // Compact pose indices and binds may change with mesh/LOD. Never reuse them.
 auto RestoreAfterCache = MoveTemp(PendingRestore);
 Clear();
 PendingRestore = MoveTemp(RestoreAfterCache);
 Source.CacheBones(Context);
}

void FRangeGroundInertial::Update_AnyThread(const FAnimationUpdateContext& Context)
{
 Source.Update(Context);
 const float Delta = Context.GetDeltaTime();
 if (FMath::IsFinite(Delta) && Delta > 0.f) AccumulatedDelta += Delta;
}

void FRangeGroundInertial::Evaluate_AnyThread(FPoseContext& Output)
{
 Source.Evaluate(Output);
 const float Delta = AccumulatedDelta;
 AccumulatedDelta = 0.f;
 if (!Enabled) { Clear(); return; }

 const int32 Count = Output.Pose.GetNumBones();
 if (OutputPrevious.Num() != Count && HistoryFrames > 0) Clear();

 TArray<FTransform> Input;
 Input.SetNumUninitialized(Count);
 for (int32 Index = 0; Index < Count; ++Index)
  Input[Index] = Output.Pose[FCompactPoseBoneIndex(Index)];

 if (BoneNames.Num() != Count)
 {
  BoneNames.SetNum(Count); ParentNames.SetNum(Count);
  const auto& Container = Output.Pose.GetBoneContainer();
  const auto& Ref = Container.GetReferenceSkeleton();
  for (int32 Index = 0; Index < Count; ++Index)
  {
   const int32 MeshIndex = Container.MakeMeshPoseIndex(FCompactPoseBoneIndex(Index)).GetInt();
   BoneNames[Index] = Ref.GetBoneName(MeshIndex);
   const int32 Parent = Ref.GetParentIndex(MeshIndex);
   ParentNames[Index] = Parent >= 0 ? Ref.GetBoneName(Parent) : NAME_None;
  }
 }

 const int32 TorsoIndex=TorsoComponentSpace?BoneNames.IndexOfByKey(TEXT("Torso")):INDEX_NONE;
 FQuat TorsoComponent=FQuat::Identity;
 if(TorsoIndex!=INDEX_NONE)
 {
  TorsoComponent=RangeTorsoComponent::Rotation(Output.Pose,FCompactPoseBoneIndex(TorsoIndex));
  // Only rotation changes representation. Native local TRS and every other
  // bone retain the existing public-engine inertialization path.
  Input[TorsoIndex].SetRotation(TorsoComponent);
 }

 if (PendingRestore)
 {
  const auto Snapshot = MoveTemp(PendingRestore);
  // Start from new source translations/scales/bind and new compact ordering.
  // Missing/nonmatching bones keep current input with zero angular history.
  InputPrevious = InputOlder = OutputPrevious = OutputOlder = Input;
  Offsets.Init(FTransform::Identity, Count);
  VelocityOffsets.Init(FSpringTransformVelocity(), Count);
  int32 Matched = 0;
  for (const auto& Bone : Snapshot->Bones)
  {
   const int32 Index = BoneNames.IndexOfByKey(Bone.Name);
   if (Index == INDEX_NONE || ParentNames[Index] != Bone.Parent) continue;
   InputPrevious[Index].SetRotation(Bone.InputPrevious);
   InputOlder[Index].SetRotation(Bone.InputOlder);
   OutputPrevious[Index].SetRotation(Bone.OutputPrevious);
   OutputOlder[Index].SetRotation(Bone.OutputOlder);
   Offsets[Index].SetRotation(Bone.Offset);
   VelocityOffsets[Index].AngularVelocity = Bone.AngularVelocityOffset;
   ++Matched;
  }
  if (Matched > 0)
  {
   PreviousDelta = Snapshot->PreviousDelta;
   if (Snapshot->PendingDuration > 0.f)
    PendingDuration = PendingDuration > 0.f ? FMath::Min(PendingDuration, Snapshot->PendingDuration) : Snapshot->PendingDuration;
   BlendDuration = Snapshot->BlendDuration;
   Elapsed = Snapshot->Elapsed;
   HistoryFrames = Snapshot->HistoryFrames;
   Active = Snapshot->Active && Elapsed < BlendDuration;
  }
 }

 // Zero-delta reevaluation must neither advance history nor consume a request.
 // It can still evaluate the same active offset against the current source.
 const bool CanAdvance = Delta > UE_SMALL_NUMBER;
 bool Started = false;
 if (PendingDuration > 0.f && CanAdvance)
 {
  if (HistoryFrames >= 2 && PreviousDelta > UE_SMALL_NUMBER)
  {
   BlendDuration = PendingDuration;
   Elapsed = 0.f;
   Offsets.SetNum(Count);
   VelocityOffsets.SetNum(Count);
   for (int32 Index = 0; Index < Count; ++Index)
   {
    FTransform Previous = OutputOlder[Index];
    FSpringTransformVelocity OutVelocity;
    UBlueprintSpringMathLibrary::TrackVelocityTransform(
     Previous, OutVelocity, OutputPrevious[Index], PreviousDelta);

    // This source is an already evolving blend, not a newly activated clip.
    // Match relative angular velocity; adding outgoing velocity to the moving
    // target would double it after the first continuity frame.
    FTransform PreviousInput = InputPrevious[Index];
    FSpringTransformVelocity InVelocity;
    UBlueprintSpringMathLibrary::TrackVelocityTransform(
     PreviousInput, InVelocity, Input[Index], Delta);
    Offsets[Index] = FTransform::Identity;
    VelocityOffsets[Index] = FSpringTransformVelocity();
    UBlueprintSpringMathLibrary::CubicInertializeTransitionTransform(
     Offsets[Index], VelocityOffsets[Index], OutputPrevious[Index], OutVelocity,
     Input[Index], InVelocity, 0.f, BlendDuration);
   }
   Active = true;
   Started = true;
  }
  PendingDuration = 0.f; // Insufficient history deliberately passes through.
 }

 if (Active)
 {
  if (CanAdvance && !Started) Elapsed += Delta;
  if (Elapsed >= BlendDuration)
  {
   Active = false;
   Offsets.Reset(); VelocityOffsets.Reset();
  }
  else
  {
   for (int32 Index = 0; Index < Count; ++Index)
   {
    FTransform Blended = UBlueprintSpringMathLibrary::CubicInertializeApplyToTransform(
     Input[Index], Offsets[Index], VelocityOffsets[Index], Elapsed, BlendDuration);
    // Other rotations remain parent-local; the isolated Torso component
    // rotation is registered after the pelvis has its final rotation. Keeping
    // child translations/scales unchanged preserves authored bone lengths.
    if (!(InertializeRootTranslation && Index == 0))
     Blended.SetTranslation(Input[Index].GetTranslation());
    Blended.SetScale3D(Input[Index].GetScale3D());
    Blended.NormalizeRotation();
    if(Index==TorsoIndex)TorsoComponent=Blended.GetRotation();
    else Output.Pose[FCompactPoseBoneIndex(Index)] = Blended;
   }
  }
 }

 if(TorsoIndex!=INDEX_NONE)
  RangeTorsoComponent::Register(Output.Pose,FCompactPoseBoneIndex(TorsoIndex),TorsoComponent);

 if (CanAdvance || HistoryFrames == 0)
 {
  InputOlder = MoveTemp(InputPrevious);
  InputPrevious = MoveTemp(Input);
  OutputOlder = MoveTemp(OutputPrevious);
  OutputPrevious.SetNumUninitialized(Count);
  for (int32 Index = 0; Index < Count; ++Index)
   OutputPrevious[Index] = Output.Pose[FCompactPoseBoneIndex(Index)];
  if(TorsoIndex!=INDEX_NONE)OutputPrevious[TorsoIndex].SetRotation(TorsoComponent);
  PreviousDelta = Delta;
  HistoryFrames = FMath::Min(HistoryFrames + 1, 2);
 }
}
