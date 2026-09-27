#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "WholeCarryReview.generated.h"

UCLASS()
class UWholeCarryMesh : public UPoseableMeshComponent
{
    GENERATED_BODY()
public:
    void SetLocalFrame(const TArray<FTransform>& Frame);
};
struct FWholeCarryClip
{
    TArray<FName> Names;
    TArray<TArray<FTransform>> Frames;
    bool Load(const FString& Directory,const FString& Name);
};
UCLASS()
class AWholeCarryReview : public AActor
{
    GENERATED_BODY()
    UPROPERTY() TArray<TObjectPtr<UWholeCarryMesh>> Bodies;
    UPROPERTY() TArray<TObjectPtr<UWholeCarryMesh>> Guns;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<class USkeletalMeshComponent> SourceGun;
    TArray<FWholeCarryClip> Clips;
    TArray<FVector> Travel;
    TArray<float> Speeds;
    TSet<FKey> PendingKeys;
    double Clock=0;
    float LegScale=1.f;
    FString ReviewDirectory;
    int32 View=0,ShotIndex=0;
    bool Capture=false;
    bool Pressed(class APlayerController* PC,const FKey& Key);
public:
    AWholeCarryReview();
    bool Initialize(class AAlsCharacter* Source);
    virtual void Tick(float Delta) override;
    bool Paused=false,Slow=false,Comparison=true,Proportions=false,Closeup=false;
    float Time=0,Speed=0;
};
UCLASS()
class AWholeCarryReviewHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
