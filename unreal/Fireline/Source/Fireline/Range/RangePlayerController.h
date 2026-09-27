#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RangePlayerController.generated.h"

struct FRangePreferences
{
 float Sensitivity=.05f,Volume=1.f;
 float PadLookSpeed=240.f,PadADSScale=.45f,PadMoveDeadZone=.15f,PadLookDeadZone=.12f;
 bool PadInvertY=false;
 int32 Quality=-1;
 bool Fullscreen=false,VSync=false;
 bool CoyoteSight=true;
};
class SRangeMenu;
class FRangeRawMouse;

// Mac gameplay consumes GCMouse floats; audit fixtures are explicitly separate from hardware evidence.
UCLASS()
class FIRELINE_API ARangePlayerController : public APlayerController
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 virtual void PlayerTick(float Dt) override;
 virtual bool InputKey(const FInputKeyEventArgs& Params) override;
 bool InjectGameplayAuditKey(const FInputKeyEventArgs& Params);
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 void InitializeGamepad();
 void TickGamepadAudit(float Dt);
 int PadAuditFrame=0,PadAuditFailures=0,PadAuditChecks=0;
 void TickGamepad(float Dt);
 void ResetGamepad();
 FVector2D PadMove=FVector2D::ZeroVector;
 bool PadFire=false,PadAim=false,PadSprint=false,PadNeedsNeutral=true;
 uint32 PadButtons=0;
 void InitializeMenu();
 void ShowMenu(bool PauseMenu=false);
 void ResumeTraining();
 UPROPERTY() TObjectPtr<class ARangeLobby> Lobby;
 bool ApplyPreferences(const FRangePreferences& Values,bool Save=true);
 bool UsesNativeRawMouse() const{return RawMouse.IsValid();}
 FString RawMouseStatus() const;
 void StopRawMouse();
 void TickRawMouse(float Dt);
 TSharedPtr<FRangeRawMouse> RawMouse;
 bool bRawMouseAudit=false;
 int32 RawAuditFrame=0;
 double RawExpectedYaw=0,RawExpectedPitch=0,RawMaxError=0;
 bool IsMenuOpen() const{return MenuWidget.IsValid();}
 FRangePreferences Preferences;
 TSharedPtr<SRangeMenu> MenuWidget;
 bool bTrainingStarted=false,bMenuAudit=false;
 float MenuAuditTime=0;
 int32 MenuAuditStep=0;
 FQuat LobbyAuditHead,LobbyAuditChest;
 float LobbyAuditBreathMax=0;
 void TickMenuAudit(float Dt);
private:
 bool bInputAudit=false, bInjecting=false;
 bool bIsolateGameplayAudit=false;
 int32 AuditFrame=0;
 float Warmup=0;
 double ExpectedYaw=0, ExpectedPitch=0, MaxError=0;
 FString Samples=TEXT("frame,dt_ms,raw_x,raw_y,yaw,pitch,expected_yaw,expected_pitch,error\n");
};
