#pragma once

#include "Animation/AnimNodeBase.h"
#include "Kismet/BlueprintSpringMathLibrary.h"

struct FRangeGroundInertialSnapshot;

// A hand-built graph node using the public Engine cubic inertialization math.
// It does not require generated AnimBlueprint NodeData or trace registration.
// Curves/attributes remain those of Source, including authored foot contacts.
struct FRangeGroundInertial : FAnimNode_Base
{
 FPoseLink Source;
 bool Enabled = false;
 bool ShareUpperBody = false;
 // SourceBody only: Torso history is a shared component orientation. Register
 // it back under the final pelvis after all other local rotations are solved.
 bool TorsoComponentSpace = false;
 // Local child translations/scales are always preserved. Root has no parent
 // bone length, so its translation may optionally be inertialized as well.
 bool InertializeRootTranslation = false;

 // Call from the owning animation update thread before this node evaluates.
 // Multiple requests before evaluation choose the shortest positive duration.
 // A nonpositive/invalid request clears the active blend and pose history.
 void Request(float Duration);
 void Clear();
 // Owning thread only, after pending animation work has completed. Snapshot
 // contains named lower-body rotations only; no compact indices or old binds.
 TSharedPtr<FRangeGroundInertialSnapshot> Capture() const;
 // Applied against the new bone container on its first source evaluation.
 void Restore(const TSharedPtr<FRangeGroundInertialSnapshot>& Snapshot);

 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
 virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
 virtual void Evaluate_AnyThread(FPoseContext& Output) override;
 virtual bool NeedsDynamicReset() const override { return true; }
 virtual void ResetDynamics(ETeleportType) override { Clear(); }

private:
 TSharedPtr<FRangeGroundInertialSnapshot> PendingRestore;
 TArray<FName> BoneNames, ParentNames;
 TArray<FTransform> InputPrevious, InputOlder;
 TArray<FTransform> OutputPrevious, OutputOlder;
 TArray<FTransform> Offsets;
 TArray<FSpringTransformVelocity> VelocityOffsets;
 float AccumulatedDelta = 0.f;
 float PreviousDelta = 0.f;
 float PendingDuration = 0.f;
 float BlendDuration = 0.f;
 float Elapsed = 0.f;
 int32 HistoryFrames = 0;
 bool Active = false;
};
