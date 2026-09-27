#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "RangeViewMesh.generated.h"

// View meshes have no physics bodies, so the default skeletal bounds describe
// the imported T pose instead of the animated grip. Track each weighted bone.
UCLASS()
class FIRELINE_API URangeViewMesh : public USkeletalMeshComponent
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 virtual void SetSkeletalMesh(USkeletalMesh* NewMesh,bool bReinitPose=true) override;
 virtual void UpdateFollowerComponent() override;
 virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
private:
 struct FBoneEnvelope { int32 Bone; FBox LocalBox; };
 TArray<FBoneEnvelope> BoneEnvelopes;
 void BuildBoneEnvelopes();
};
