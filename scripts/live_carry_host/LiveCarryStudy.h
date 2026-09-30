#pragma once
#include "Range/RangeGame.h"
#include "Components/PoseableMeshComponent.h"
#include "LiveCarryRetarget.h"
#include "LiveCarryStudy.generated.h"

UCLASS()
class ULiveCarryMesh : public UPoseableMeshComponent
{
    GENERATED_BODY()
public:
    void ApplyFrame(const TArray<FTransform>& Local,const TArray<FName>& Names);
};

// Inherits the existing Fireline capsule and CharacterMovement constructor.
// Deliberately bypasses weapon/action presentation in this scoped locomotion
// test so there is only one writer of the displayed full-body pose.
UCLASS()
class ALiveCarryPawn : public ARangeCharacter
{
    GENERATED_BODY()
public:
    ALiveCarryPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    UPROPERTY() TObjectPtr<ULiveCarryMesh> StudyBody;
    UPROPERTY() TObjectPtr<ULiveCarryMesh> StudyGun;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> StudyOptic;
    bool CoyoteStudy=false,ShowCoyote=true,ContactGuides=false,CloseContactView=true;
    double SightProxyErrorCm=0,OpticMountErrorCm=0;
    FTransform OpticMount;
    FVector EyeHeadLocal,StockCenterLocal,StockCornerLocal;
    FString OpticRows;
    bool InitializeStudyOptic();
    void UpdateStudyOptic(double Time);
    void ExportStudyOptic(const FString& Folder);
    void ToggleStudyOptic(){if(CoyoteStudy&&!Audit)ShowCoyote=!ShowCoyote;}
    void ToggleContactGuides(){if(CoyoteStudy&&!Audit)ContactGuides=!ContactGuides;}
    void ToggleContactCloseup(){if(CoyoteStudy&&!Audit)CloseContactView=!CloseContactView;}
    UPROPERTY() TObjectPtr<class ACameraActor> StudyCamera;
    bool ForwardRequested=false,Ready=false,Audit=false;
    bool Directional=false,JogRequested=false;
    bool AimStudy=false,AimRequested=false;
    bool AimLatched=false,Demo=false,PresentationStudy=false;
    double DemoElapsed=0;
    void ToggleAim(){if(AimStudy&&!Audit)AimLatched=!AimLatched;}
    void ToggleDemo(){if(AimStudy&&!Audit){Demo=!Demo;DemoElapsed=0;AimLatched=false;if(Demo)ResetStudy();}}
    float AimPitch=0;
    bool ShowSource=false;
    FVector SourceFocus=FVector::ZeroVector;
    void ToggleSource(){if(AimStudy)ShowSource=!ShowSource;}
    FString RifleState;
    float FacingYaw=0,LegScale=1;
    FVector MoveRequested=FVector::ZeroVector;
    int Scenario=0;
    float OrbitYaw=0,OrbitPitch=8,OrbitDistance=510;
    void ForwardDown();
    void ForwardUp();
    void LeftDown();
    void RightDown();
    void BackDown();
    void RequestTap(const FVector& Direction);
    void OrbitX(float Value);
    void OrbitY(float Value);
    void Front(){OrbitYaw=0+(Directional?GetActorRotation().Yaw:0);}
    void Right(){OrbitYaw=90+(Directional?GetActorRotation().Yaw:0);}
    void Back(){OrbitYaw=180+(Directional?GetActorRotation().Yaw:0);}
    void Left(){OrbitYaw=270+(Directional?GetActorRotation().Yaw:0);}
    void ResetStudy();
    void UpdateCamera();
};

UCLASS()
class ALiveCarryHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

class FLiveCarryStudy
{
    TWeakObjectPtr<class AAlsCharacter> Source;
    TWeakObjectPtr<ALiveCarryPawn> Pawn;
    FLiveCarryRetarget Retarget;
    TArray<FName> SourceNames;
    TArray<FTransform> LastLocal,LastComponent;
    double Start=-1,WarmStart=-1;
    FVector PawnOrigin,SourceOrigin;
    int32 Frame=0,Failures=0,Shot=0,FPS=60;
    bool Finished=false,Audit=false,Parity=false;
    bool Directional=false;
    bool AimStudy=false;
    double RelaxedWeight=1,ReadyWeight=0,AimingWeight=0;
    FString RawRows;
    void AimInput(class UWorld* World,float Dt,double Time);
    void ReadRifleStates();
    void ConfigurePresentationTransitions();
    FString Folder,PoseRows,StateRows,SourceRows;
    void Record(float Dt,double T);
    void DirectionalInput(class UWorld* World,float Dt,double Time);
    double DirectionalJogWeight();
    void DirectionalSource(class AAlsCharacter* Character,ALiveCarryPawn* Player,float Dt);
public:
    void Before(class UWorld* World,float Dt);
    void After(class UWorld* World,float Dt);
};
