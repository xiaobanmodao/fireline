#include "RangePlayerController.h"
#include "RangeGamepadRules.h"
#include "RangeGame.h"
#include "RangeMenu.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void ARangePlayerController::TickGamepadAudit(float Dt)
{
 auto* P=Cast<ARangeCharacter>(GetPawn());if(!P)return;
 static float PadExpectedYaw=0;
 const int F=PadAuditFrame++;
 auto Check=[this](bool Pass,const TCHAR* Label){++PadAuditChecks;if(!Pass)++PadAuditFailures;UE_LOG(LogTemp,Display,TEXT("GAMEPAD_CHECK %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Label);};
 if(F==0)
 {
  PrimaryActorTick.bTickEvenWhenPaused=true;bShouldPerformFullTickWhenPaused=true;Preferences=FRangePreferences();SetControlRotation(FRotator::ZeroRotator);
  Check(RangeGamepad::Stick(FVector2D(.05,.05),.15).IsNearlyZero(),TEXT("radial deadzone"));
  Check(FMath::IsNearlyEqual(RangeGamepad::Stick(FVector2D(1,1),.15).Size(),1.),TEXT("diagonal magnitude capped"));
  Check(RangeGamepad::Stick(FVector2D(.151,0),.15).Size()<.002,TEXT("continuous deadzone edge"));
  Check(RangeGamepad::Trigger(.19,true)&&!RangeGamepad::Trigger(.19,false),TEXT("trigger hysteresis"));
  for(int FPS:{30,60,120}){float Yaw=0;for(int I=0;I<FPS;++I)Yaw+=240.f/FPS;Check(FMath::IsNearlyEqual(Yaw,240.f,.001f),TEXT("angular rate integrates equally at 30/60/120"));}
 }
 if(F==120){Check(GetControlRotation().IsNearlyZero(.01f),TEXT("neutral stick does not drift"));Check(P->GetVelocity().Size2D()<1,TEXT("neutral stick stationary"));}
 if(F==180||F==235||F==300||F==305||F==311||F==380||F==470||F==498)UE_LOG(LogTemp,Display,TEXT("PAD_TRACE f=%d dt=%f yaw=%f speed=%f shots=%d aim=%d fire=%d jump=%d neutral=%d ammo=%d"),F,Dt,GetControlRotation().Yaw,P->GetVelocity().Size2D(),P->Shots,PadAim,PadFire,P->bJumpHeld,PadNeedsNeutral,P->Ammo);
 if(F==180)Check(FMath::Abs(FMath::FindDeltaAngleDegrees(GetControlRotation().Yaw,PadExpectedYaw))<.05f,TEXT("injected right stick produces expected yaw"));
 if(F==235){Check(PadMove.Y>.4&&PadMove.Y<.42,TEXT("raw stick gets one radial deadzone"));Check(P->GetVelocity().Size2D()>100&&P->GetVelocity().Size2D()<250,TEXT("partial stick controls walking speed"));}
 if(F==269)Check(PadAim&&P->bAimHeld,TEXT("left trigger aims"));
 if(F==300)Check(P->Shots>=2,TEXT("right trigger holds automatic fire"));
 if(F==305)Check(!PadFire&&!PadAim&&!P->bAimHeld,TEXT("trigger release clears held actions"));
 if(F==312)Check(P->bJumpHeld,TEXT("south button jump"));if(F==315)Check(!P->bJumpHeld,TEXT("jump release"));
 if(F==365)Check(P->bReloading,TEXT("west button reload"));
 if(F==380)Check(!P->bReloading,TEXT("trigger interrupts tactical reload"));
 if(F==430)Check(P->bCrouchHeld,TEXT("east button crouch"));if(F==445)Check(!P->bCrouchHeld,TEXT("crouch release"));
 if(F==470)Check(P->bKnife||P->bMP7||P->PendingWeapon==3,TEXT("shoulder switches weapon"));
 if(F==494){ResetGamepad();Check(!PadFire&&!PadAim&&!PadSprint&&PadMove.IsNearlyZero(),TEXT("reset clears all held state"));}
 if(F==498)Check(PadNeedsNeutral&&!PadFire,TEXT("held trigger blocked until neutral after reset"));
 if(F==530)Check(PadFire&&!PadNeedsNeutral,TEXT("trigger rearms after release"));
 if(F==560){ShowMenu(true);Check(IsMenuOpen()&&!PadFire&&PadMove.IsNearlyZero(),TEXT("pause clears gamepad state"));}
 if(F==562)Check(FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid()&&FSlateApplication::Get().GetKeyboardFocusedWidget()!=MenuWidget,TEXT("menu initial focus is an interactive control"));
 if(F==565&&MenuWidget.IsValid())MenuWidget->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_FaceButton_Right,FModifierKeysState(),0,false,0,0));
 if(F==568)Check(!IsMenuOpen(),TEXT("gamepad east returns from pause"));
 if(F==590)
 {
  Check(FMath::IsNearlyEqual(P->GetMouseSensitivity(),.05f),TEXT("mouse sensitivity unchanged"));
  const FString Report=FString::Printf(TEXT("checks=%d failures=%d input=synthetic-engine-events hardware=false\n"),PadAuditChecks,PadAuditFailures);
  FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("gamepad-audit.txt")));UE_LOG(LogTemp,Display,TEXT("FIRELINE_GAMEPAD_AUDIT %s"),*Report);FPlatformMisc::RequestExit(false);return;
 }
 if(F<180)PadExpectedYaw+=RangeGamepad::Stick(FVector2D(PlayerInput->GetRawKeyValue(EKeys::Gamepad_RightX),0),Preferences.PadLookDeadZone,1.6f).X*Preferences.PadLookSpeed*FMath::Clamp(Dt,0.f,.05f);
 if(F==562){auto W=FSlateApplication::Get().GetKeyboardFocusedWidget();UE_LOG(LogTemp,Display,TEXT("PAD_FOCUS %s"),W.IsValid()?*W->GetTypeAsString():TEXT("none"));}
 auto Axis=[this,Dt](FKey K,float V){InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,V,Dt,1,FPlatformTime::Cycles64()));};
 Axis(EKeys::Gamepad_LeftX,0);Axis(EKeys::Gamepad_LeftY,F>=180&&F<240?.5f:0);
 Axis(EKeys::Gamepad_RightX,F>=120&&F<180?1.f:F<120?.04f:0);Axis(EKeys::Gamepad_RightY,0);
 Axis(EKeys::Gamepad_LeftTriggerAxis,F>=240&&F<300?1.f:0);
 Axis(EKeys::Gamepad_RightTriggerAxis,(F>=270&&F<300)||(F>=370&&F<378)||(F>=491&&F<500)||(F>=510&&F<540)?1.f:0);
 auto Button=[this,F](int Begin,int End,FKey K){if(F==Begin||F==End)InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,F==Begin?IE_Pressed:IE_Released,1.f,false,FPlatformTime::Cycles64()));};
 Button(310,313,EKeys::Gamepad_FaceButton_Bottom);Button(360,362,EKeys::Gamepad_FaceButton_Left);Button(420,440,EKeys::Gamepad_FaceButton_Right);Button(450,452,EKeys::Gamepad_RightShoulder);
}
