#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "LyraADSReview.generated.h"

UCLASS()
class ALyraADSReview : public AActor
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Body;
    UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Gun;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<class UAnimSequence> Clip;
    FString Directory,Poses,GunPoses;
    double Clock=0;
    int Frame=0,Shot=0,View=0;
    bool Capture=false,Closeup=true;
    bool ToggleAudit=false;
    int ToggleStage=0;
    float Yaw=0,Pitch=5;
    void SetFront(){Yaw=0;}void SetRight(){Yaw=-90;}void SetBack(){Yaw=180;}void SetLeft(){Yaw=90;}
    void OrbitX(float V){Yaw+=V*.3f;}void OrbitY(float V){Pitch=FMath::Clamp(Pitch+V*.2f,-25.f,45.f);}
    void TogglePause(){Paused=!Paused;}void ToggleClose(){Closeup=!Closeup;}
    void TogglePostProcess();
public:
    ALyraADSReview();
    bool Initialize(class AAlsCharacter* Source);
    virtual void Tick(float Delta) override;
    bool Paused=false;
    bool RawClip=false;
    double AnimationTime=0;
};
UCLASS()
class ALyraADSReviewHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
