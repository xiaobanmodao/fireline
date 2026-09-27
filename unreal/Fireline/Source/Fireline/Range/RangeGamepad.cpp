#include "RangePlayerController.h"
#include "RangeGame.h"
#include "RangeGamepadRules.h"
#include "GameFramework/PlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "UnrealClient.h"

void ARangePlayerController::InitializeGamepad()
{
 IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,[this](EInputDeviceConnectionState State,FPlatformUserId User,FInputDeviceId)
 {
  if(State==EInputDeviceConnectionState::Disconnected&&User==GetPlatformUserId())
  {ResetGamepad();if(PlayerInput)PlayerInput->FlushPressedKeys();}
 });
}
void ARangePlayerController::ResetGamepad()
{
 if(auto* P=Cast<ARangeCharacter>(GetPawn()))
 {
  if(PadButtons&1)P->EndJump();if(PadButtons&2)P->EndCrouch();
  if(PadAim&&!IsInputKeyDown(EKeys::RightMouseButton))P->EndAim();
 }
 PadNeedsNeutral=true;PadButtons=0;PadAim=PadFire=PadSprint=false;PadMove=FVector2D::ZeroVector;
}
void ARangePlayerController::TickGamepad(float Dt)
{
 const bool Audit=FParse::Param(FCommandLine::Get(),TEXT("FirelineGamepadAudit"));
 const bool Focused=FSlateApplication::IsInitialized()&&FSlateApplication::Get().IsActive()&&GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport&&GEngine->GameViewport->Viewport->HasFocus();
 auto* P=Cast<ARangeCharacter>(GetPawn());
 if(!P||!PlayerInput||IsMenuOpen()||IsPaused()||(!Focused&&!Audit)||(FString(FCommandLine::Get()).Contains(TEXT("Audit"))&&!Audit)){ResetGamepad();return;}
 auto Raw=[this](FKey K){return PlayerInput->GetRawKeyValue(K);};
 if(PadNeedsNeutral)
 {
  if(Raw(EKeys::Gamepad_LeftTriggerAxis)>.12f||Raw(EKeys::Gamepad_RightTriggerAxis)>.12f)return;
  for(FKey K:{EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_FaceButton_Left,EKeys::Gamepad_FaceButton_Top,EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_RightShoulder,EKeys::Gamepad_LeftThumbstick,EKeys::Gamepad_RightThumbstick,EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_Special_Left})if(IsInputKeyDown(K))return;
  PadNeedsNeutral=false;
 }
 PadMove=RangeGamepad::Stick(FVector2D(Raw(EKeys::Gamepad_LeftX),Raw(EKeys::Gamepad_LeftY)),Preferences.PadMoveDeadZone);
 const FVector2D Look=RangeGamepad::Stick(FVector2D(Raw(EKeys::Gamepad_RightX),Raw(EKeys::Gamepad_RightY)),Preferences.PadLookDeadZone,1.6f);
 const float Gain=Preferences.PadLookSpeed*FMath::Lerp(1.f,Preferences.PadADSScale,P->AimAlpha)*FMath::Clamp(Dt,0.f,.05f);
 AddYawInput(Look.X*Gain);AddPitchInput(Look.Y*Gain*.75f*(Preferences.PadInvertY?1.f:-1.f));
 const bool Aim=RangeGamepad::Trigger(Raw(EKeys::Gamepad_LeftTriggerAxis),PadAim);
 if(Aim&&!P->bAimHeld)P->StartAim();else if(!Aim&&PadAim&&!IsInputKeyDown(EKeys::RightMouseButton))P->EndAim();PadAim=Aim;
 const bool Fire=RangeGamepad::Trigger(Raw(EKeys::Gamepad_RightTriggerAxis),PadFire);
 if(Fire&&!PadFire)P->KnifeAttackPressed();PadFire=Fire;
 const FKey Keys[]={EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_FaceButton_Left,EKeys::Gamepad_FaceButton_Top,EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_RightShoulder,EKeys::Gamepad_LeftThumbstick,EKeys::Gamepad_RightThumbstick,EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_Special_Left};
 uint32 Buttons=0;for(int I=0;I<UE_ARRAY_COUNT(Keys);++I)if(IsInputKeyDown(Keys[I]))Buttons|=1u<<I;
 const uint32 Pressed=Buttons&~PadButtons,Released=PadButtons&~Buttons;
 if(Pressed&1)P->StartJump();if(Released&1)P->EndJump();
 if(Pressed&2)P->StartCrouch();if(Released&2)P->EndCrouch();
 if(Pressed&4)P->Reload();if(Pressed&8)P->CycleWeapon();
 if(Pressed&16)P->CycleWeaponBack();if(Pressed&32)P->CycleWeapon();
 if(Pressed&64)PadSprint=!PadSprint;if(PadMove.Size()<.15f)PadSprint=false;
 if(Pressed&128)P->EquipKnife();if(Pressed&256)P->TestFingers();if(Pressed&512)P->ToggleEmptyHands();if(Pressed&1024)P->ToggleView();
 PadButtons=Buttons;
}
