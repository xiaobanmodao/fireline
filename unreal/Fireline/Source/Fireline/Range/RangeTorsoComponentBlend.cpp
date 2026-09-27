#include "RangeTorsoComponentBlend.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

void FRangeTorsoComponentBlend::Evaluate_AnyThread(FPoseContext& Output)
{
 if(!RegisterTorsoComponent||!bAIsRelevant||!bBIsRelevant)
 {
  FAnimNode_TwoWayBlend::Evaluate_AnyThread(Output);
  return;
 }
 // Evaluate each input once. Do not reevaluate children to recover component
 // rotations: that would consume stateful animation/constraint history twice.
 A.Evaluate(Output);
 FPoseContext Other(Output);B.Evaluate(Other);
 const auto& Container=Output.Pose.GetBoneContainer();
 const int32 MeshIndex=Container.GetReferenceSkeleton().FindBoneIndex(TEXT("Torso"));
 const auto Torso=MeshIndex>=0?Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)):FCompactPoseBoneIndex(INDEX_NONE);
 FQuat Component=FQuat::Identity;
 if(Torso.GetInt()!=INDEX_NONE)
  Component=FQuat::Slerp(RangeTorsoComponent::Rotation(Output.Pose,Torso),RangeTorsoComponent::Rotation(Other.Pose,Torso),InternalBlendAlpha).GetNormalized();
 FAnimationPoseData InOut(Output);const FAnimationPoseData OtherData(Other);
 FAnimationRuntime::BlendTwoPosesTogetherInPlace(InOut,OtherData,1.f-InternalBlendAlpha);
 if(Torso.GetInt()!=INDEX_NONE)RangeTorsoComponent::Register(Output.Pose,Torso,Component);
}
