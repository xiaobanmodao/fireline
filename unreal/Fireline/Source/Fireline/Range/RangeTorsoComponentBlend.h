#pragma once

#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "BonePose.h"

// Same update/relevancy, local TRS blend, curves and attributes as TwoWayBlend.
// The isolated source-body graph additionally blends its upper-chain root in
// the shared component frame, then registers it to the blended native pelvis.
struct FRangeTorsoComponentBlend : FAnimNode_TwoWayBlend
{
 bool RegisterTorsoComponent = false;
 virtual void Evaluate_AnyThread(FPoseContext& Output) override;
};

namespace RangeTorsoComponent
{
 // Rotation only: native local translations/scales never enter registration.
 inline FQuat Rotation(const FCompactPose& Pose,FCompactPoseBoneIndex Index)
 {
  FQuat Q=Pose[Index].GetRotation();
  for(Index=Pose.GetParentBoneIndex(Index);Index.GetInt()!=INDEX_NONE;Index=Pose.GetParentBoneIndex(Index))
   Q=(Pose[Index].GetRotation()*Q).GetNormalized();
  return Q;
 }
 inline void Register(FCompactPose& Pose,FCompactPoseBoneIndex Torso,const FQuat& Component)
 {
  const auto Parent=Pose.GetParentBoneIndex(Torso);
  const FQuat ParentRotation=Parent.GetInt()!=INDEX_NONE?Rotation(Pose,Parent):FQuat::Identity;
  Pose[Torso].SetRotation((ParentRotation.Inverse()*Component).GetNormalized());
 }
}
