#include "RangePlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "RangeMenu.h"
#include "TimerManager.h"
#include "RangeRawMouse.h"
#include "RangeGame.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
void ARangePlayerController::BeginPlay()
{
 Super::BeginPlay();InitializeGamepad();
 bInputAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineInputAudit"));
 bRawMouseAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineRawMouseAudit"));
 bMenuAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineMenuAudit"));
 bIsolateGameplayAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineMagazineDropAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyPoseDiagnosisAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineM4AnimationAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementSpreadAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponHandlingAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponPolishAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadCasingAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineFireInterruptAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponControlsAudit"));
 if(!FString(FCommandLine::Get()).Contains(TEXT("Audit"))||bRawMouseAudit||bMenuAudit)RawMouse=FRangeRawMouse::Create(bRawMouseAudit);
 if(!FString(FCommandLine::Get()).Contains(TEXT("Audit"))||bMenuAudit)
 {FTimerHandle MenuTimer;GetWorldTimerManager().SetTimer(MenuTimer,this,&ARangePlayerController::InitializeMenu,.35f,false);}
}
bool ARangePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
 if(bIsolateGameplayAudit)return true;
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineShieldAudit")))return true;
 if(bInputAudit&&!bInjecting)return true;
 if(!bInputAudit&&(Params.Key==EKeys::Escape||Params.Key==EKeys::Gamepad_Special_Right)&&Params.Event==IE_Pressed&&!IsMenuOpen()){ShowMenu(true);return true;}
 return Super::InputKey(Params);
}
bool ARangePlayerController::InjectGameplayAuditKey(const FInputKeyEventArgs& Params)
{
 // Run the real binding/state path without letting desktop mouse activity
 // accidentally cancel a timed reload during automated checks.
 return bIsolateGameplayAudit?Super::InputKey(Params):InputKey(Params);
}
void ARangePlayerController::PlayerTick(float Dt)
{
 if(!bInputAudit)
 {
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineGamepadAudit")))TickGamepadAudit(Dt);
  TickRawMouse(Dt);TickGamepad(Dt);Super::PlayerTick(Dt);
  if(bRawMouseAudit&&RawMouse)
  {
   const FRotator R=GetControlRotation().GetNormalized();
   RawMaxError=FMath::Max(RawMaxError,FMath::Max(FMath::Abs(R.Yaw-RawExpectedYaw),FMath::Abs(R.Pitch-RawExpectedPitch)));
   if(++RawAuditFrame==360){UE_LOG(LogTemp,Display,TEXT("FIRELINE_RAW_AUDIT frames=360 error=%.8f events=%llu devices=%d source=GCMouse fixture=true"),RawMaxError,RawMouse->EventCount(),RawMouse->DeviceCount());FPlatformMisc::RequestExit(false);}
  }
  if(bMenuAudit)TickMenuAudit(Dt);return;
 }
 Warmup+=Dt;
 if(Warmup<6){Super::PlayerTick(Dt);SetControlRotation(FRotator::ZeroRotator);return;}
 float X=0,Y=0;
 if(AuditFrame>=60&&AuditFrame<180){X=.25f;Y=.125f;}
 else if(AuditFrame>=240&&AuditFrame<360){X=-.25f;Y=-.125f;}
 else if(AuditFrame>=360&&AuditFrame<420){X=(AuditFrame%2)?-1.f:1.f;Y=-X;}
 bInjecting=true;
 const auto Device=FInputDeviceId::CreateFromInternalId(0);
 InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseX,X,Dt,1,FPlatformTime::Cycles64()));
 InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseY,Y,Dt,1,FPlatformTime::Cycles64()));
 bInjecting=false;
 Super::PlayerTick(Dt);
 ExpectedYaw+=X*.05;ExpectedPitch+=Y*.05;
 const FRotator Rot=GetControlRotation().GetNormalized();
 const double Error=FMath::Max(FMath::Abs(Rot.Yaw-ExpectedYaw),FMath::Abs(Rot.Pitch-ExpectedPitch));
 MaxError=FMath::Max(MaxError,Error);
 Samples+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f,%.8f,%.8f,%.8f,%.8f,%.8f\n"),AuditFrame,Dt*1000,X,Y,Rot.Yaw,Rot.Pitch,ExpectedYaw,ExpectedPitch,Error);
 if(AuditFrame==240&&FParse::Param(FCommandLine::Get(),TEXT("FirelineInputCapture")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/input-material-fixed.png"),true,false);
 if(++AuditFrame==480)
 {
  IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true);
  FFileHelper::SaveStringToFile(Samples,*(FPaths::ProjectSavedDir()/TEXT("input-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_INPUT_AUDIT samples=%d max_error_degrees=%.8f yaw=%.8f pitch=%.8f"),AuditFrame,MaxError,Rot.Yaw,Rot.Pitch);
  FPlatformMisc::RequestExit(false);
 }
}

void ARangePlayerController::StopRawMouse()
{
 if(RawMouse)RawMouse->SetEnabled(false);
 RotationInput=FRotator::ZeroRotator;
}
FString ARangePlayerController::RawMouseStatus() const
{
#if PLATFORM_MAC
 return RawMouse&&RawMouse->DeviceCount()>0?TEXT("原始鼠标输入：开启 · 系统速度与加速不参与瞄准"):TEXT("原始鼠标输入：等待鼠标连接");
#else
 return TEXT("原始鼠标输入：由引擎高精度模式处理");
#endif
}
void ARangePlayerController::TickRawMouse(float Dt)
{
 if(!RawMouse)return;
 const bool Focused=FSlateApplication::IsInitialized()&&FSlateApplication::Get().IsActive()&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport&&GEngine->GameViewport->Viewport->HasFocus()&&GEngine->GameViewport->Viewport->HasMouseCapture()&&!bShowMouseCursor;
 const bool Active=bRawMouseAudit?!(RawAuditFrame>=240&&RawAuditFrame<270):Focused&&!IsMenuOpen()&&!IsPaused()&&!IsLookInputIgnored();
 // Exercise pending-input discard as well as incoming events while inactive.
 if(bRawMouseAudit&&RawAuditFrame==240)RawMouse->InjectForAudit(99,99);
 RawMouse->SetEnabled(Active);
 if(bRawMouseAudit)
 {
  double X=0,Y=0;
  if(RawAuditFrame<120){X=.125;Y=.0625;}
  else if(RawAuditFrame<240){X=-.125;Y=-.0625;}
  else if(RawAuditFrame<270){X=100;Y=100;} // Inactive input must be discarded.
  else if(RawAuditFrame<300){X=(RawAuditFrame%2)?-.25:.25;Y=-X;}
  RawMouse->InjectForAudit(X*.5,Y*.5);
  RawMouse->InjectForAudit(X*.5,Y*.5);
  if(Active){RawExpectedYaw+=X*.05;RawExpectedPitch+=Y*.05;}
  // Simulate a duplicate legacy event; the pawn must reject this look path.
  const auto Device=FInputDeviceId::CreateFromInternalId(0);
  InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseX,100.f,Dt,1,FPlatformTime::Cycles64()));
 }
 const FVector2D Delta=RawMouse->Consume();
 if(Active)if(auto* P=Cast<ARangeCharacter>(GetPawn()))P->ApplyRawMouseDelta(Delta);
}
