#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "RangeHitMesh.generated.h"

// Bullet queries against Ryan's animated low-poly surface, independent of the
// movement capsule and without enabling world collision on the visual mesh.
UCLASS()
class FIRELINE_API URangeHitMesh : public USkeletalMeshComponent
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 bool TraceSurface(const FVector& Start,const FVector& Direction,float Limit,float& Distance,bool& Headshot,FVector& Normal) const;
 FName GetLastHitBone() const { return LastHitBone; }
private:
 struct FTriangle { uint32 A,B,C; };
 TArray<FTriangle> Triangles;
 TArray<float> HeadWeights;
 TArray<int32> DominantBones;
 mutable FName LastHitBone;
 mutable TArray<FVector3f> Positions;
 mutable TArray<FMatrix44f> Matrices;
 mutable uint64 PoseFrame=MAX_uint64;
 mutable uint32 PoseRevision=0;
 mutable FBox PoseBox=FBox(ForceInit);
 bool UpdateQueryPose() const;
};
