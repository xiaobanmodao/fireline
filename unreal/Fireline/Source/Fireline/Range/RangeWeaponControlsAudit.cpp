#include "RangeGame.h"
#include "RangePlayerController.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"

void ARangeCharacter::StartWeaponControlsAudit()
{
 const bool FireInterrupt=FParse::Param(FCommandLine::Get(),TEXT("FirelineFireInterruptAudit"));
 if(!FireInterrupt&&!FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponControlsAudit")))return;
 struct FState {int32 Checks=0,Failures=0,ShotsBefore=0,InspectSerial=0;};
 auto State=MakeShared<FState>();
 auto Check=[State](bool Pass,const TCHAR* Name){++State->Checks;State->Failures+=!Pass;UE_LOG(LogTemp,Display,TEXT("WEAPON_CONTROLS_CHECK %s pass=%d"),Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Key=[this](FKey K,EInputEvent E){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
 auto Tap=[Key](FKey K){Key(K,IE_Pressed);Key(K,IE_Released);};
 auto Capture=[](const TCHAR* Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponControlsCapture")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/weapon-controls-%s.png"),Name),true,false);};
 if(FireInterrupt)
 {
  Later(3,[this,Tap]{Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=12;Tap(EKeys::R);});
  Later(3.35f,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(3.4f,[this,Check]{Check(!bReloading&&Ammo==12&&bFireAfterReloadReturn,TEXT("tap_cancels_reload_before_shot"));});
  Later(3.7f,[this,Check]{Check(Shots==1&&Ammo==11&&!bFireAfterReloadReturn,TEXT("short_tap_emits_exactly_one_shot"));});
  Later(4.2f,[Tap]{Tap(EKeys::R);});
  Later(5.65f,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(5.85f,[this,Check]{Check(!bReloading&&Shots==2&&Ammo==29,TEXT("fire_cancels_after_magazine_seated"));});
  Later(5.9f,[Key]{Key(EKeys::LeftMouseButton,IE_Pressed);});
  Later(6.15f,[Key]{Key(EKeys::LeftMouseButton,IE_Released);});
  Later(6.4f,[this,State,Check]{Check(Shots>=4&&Ammo<29,TEXT("held_fire_continues"));State->ShotsBefore=Shots;Ammo=0;});
  Later(6.7f,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(7,[this,State,Check]{Check(bReloading&&Ammo==0&&Shots==State->ShotsBefore,TEXT("empty_magazine_does_not_cancel_or_fire"));});
  Later(9.2f,[this,Check]{Check(!bReloading&&Ammo==30,TEXT("empty_reload_completes"));});
  Later(9.4f,[this,Tap]{Ammo=5;Tap(EKeys::R);});
  Later(9.8f,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(9.85f,[Tap]{Tap(EKeys::Two);});
  Later(10.6f,[this,State,Check]{Check(bKnife&&Ammo==5&&Shots==State->ShotsBefore&&!bFireAfterReloadReturn,TEXT("switch_cancels_buffered_shot"));});
  Later(11.8f,[Tap]{Tap(EKeys::One);});
  Later(12.5f,[this,Check]{Check(!bKnife&&Ammo==5&&!bReloading,TEXT("rifle_keeps_remaining_ammo"));});
  Later(12.6f,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(12.9f,[this,State,Check,Tap]{Check(Ammo==4&&Shots==State->ShotsBefore+1,TEXT("fire_works_after_switch_cancel"));Tap(EKeys::R);});
  Later(15.6f,[this,Check]{Check(!bReloading&&Ammo==30,TEXT("manual_reload_still_completes"));});
  Later(16,[this,Key,Tap]{Ammo=10;Key(EKeys::RightMouseButton,IE_Pressed);Tap(EKeys::R);});
  Later(17,[Tap]{Tap(EKeys::LeftMouseButton);});
  Later(17.4f,[this,Check,Key]{Check(!bReloading&&Ammo==9&&bAimHeld&&AimAlpha>.8f,TEXT("aim_restores_after_fire_cancel"));Key(EKeys::RightMouseButton,IE_Released);});
  Later(18,[State]{UE_LOG(LogTemp,Display,TEXT("FIRE_INTERRUPT_AUDIT checks=%d failures=%d"),State->Checks,State->Failures);FPlatformMisc::RequestExit(false);});
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineFireInterruptCapture")))
  {
   FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);
   for(int32 I=0;I<13;++I)for(int32 Phase=0;Phase<2;++Phase)
    Later((Phase?5.43f:3.3f)+I/30.f,[I,Phase]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/fire-interrupt-%d-%02d.png"),Phase,I),true,false);});
  }
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponControlsCapture")))
 {
  FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);
  for(int32 I=0;I<13;++I)
  {
   Later(21.8f+I*.05f,[Capture,I]{Capture(*FString::Printf(TEXT("restart-%02d"),I));});
   Later(25.5f+I*.05f,[Capture,I]{Capture(*FString::Printf(TEXT("cancel-%02d"),I));});
  }
 }
 Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=1;Fire();});
 Later(3.25f,[this,Check]{Check(Ammo==0&&Shots==1&&bReloading,TEXT("last_round_auto_reload"));});
 Later(3.3f,[Tap]{Tap(EKeys::MouseScrollDown);});
 Later(3.55f,[this,Check,Capture]{Check(!bReloading&&PendingWeapon==2&&Ammo==0,TEXT("wheel_cancels_reload_immediately"));Capture(TEXT("auto-reload"));});
 Later(6.1f,[this,Check,Tap]{Check(bKnife&&!bReloading&&Ammo==0&&HandSwitchLeft<=0,TEXT("cancel_does_not_refill_hidden_rifle"));Tap(EKeys::F);});
 Later(6.4f,[Tap]{Tap(EKeys::MouseScrollUp);});
 Later(7.15f,[this,Check]{Check(!bKnife&&!bEmptyHands&&Arms->GetSkeletalMeshAsset()==RifleArmsMesh,TEXT("wheel_interrupts_inspect_restores_rifle"));});
 Later(7.3f,[Tap]{Tap(EKeys::MouseScrollDown);});
 Later(7.4f,[Tap]{Tap(EKeys::MouseScrollUp);});
 Later(7.5f,[Tap]{Tap(EKeys::MouseScrollDown);});
 Later(8.6f,[this,Check]{Check(bKnife&&HandSwitchLeft<=0&&QueuedWeapon<0,TEXT("rapid_wheel_final_selection"));});
 Later(8.7f,[Tap]{Tap(EKeys::Zero);});
 Later(9.3f,[this,Check]{Ammo=0;Check(bEmptyHands&&!bReloading,TEXT("empty_hands_no_reload"));});
 Later(9.5f,[this,Check,Tap]{Check(!bReloading&&Ammo==0,TEXT("empty_hands_stays_empty"));Tap(EKeys::MouseScrollUp);});
 Later(10.25f,[this,Check]{Check(!bEmptyHands&&!bKnife&&bReloading,TEXT("equip_empty_rifle_auto_reload"));});
 Later(12.9f,[this,Check,Capture]{Check(Ammo==30&&!bReloading,TEXT("auto_reload_returns_ready"));Capture(TEXT("rifle-hud"));});
 Later(13,[this,Tap]{Fire();Tap(EKeys::R);});
 Later(13.3f,[this,Check]{Check(Ammo==29&&bReloading,TEXT("manual_reload_preserved"));});
 Later(15.9f,[this,Check]{Check(Ammo==30&&!bReloading,TEXT("manual_reload_complete"));});
 Later(16,[this,State,Key]{Ammo=1;State->ShotsBefore=Shots;Key(EKeys::LeftMouseButton,IE_Pressed);});
 Later(18.85f,[Key]{Key(EKeys::LeftMouseButton,IE_Released);});
 Later(19.1f,[this,State,Check,Capture]{Check(Shots-State->ShotsBefore>=2&&!bReloading&&Ammo>0&&Ammo<30,TEXT("held_fire_resumes_after_reload"));Capture(TEXT("ready"));});
 Later(19.3f,[this,Tap]{ToggleView();Controller->SetControlRotation(FRotator(-8,155,0));Tap(EKeys::MouseScrollDown);});
 Later(20.7f,[this,Check,Capture]{Check(bKnife&&BodyKnife->IsVisible()&&!BodyGun->IsVisible(),TEXT("third_person_wheel_visibility"));Capture(TEXT("third-hud-sign"));});
 Later(21,[this,Tap]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);Tap(EKeys::F);});
 Later(21.95f,[this,State,Tap]{State->InspectSerial=KnifeActionSerial;Tap(EKeys::F);});
 Later(22.02f,[this,State,Check,Tap]{Check(KnifeAction==2&&KnifeTime<.15f&&KnifeActionSerial==State->InspectSerial+1,TEXT("inspect_restarts_on_F"));Tap(EKeys::F);});
 Later(22.3f,[this,State,Check]{Check(KnifeAction==2&&KnifeTime<.3f&&KnifeActionSerial==State->InspectSerial+2,TEXT("repeat_F_during_blend_is_not_lost"));});
 Later(22.5f,[Tap]{Tap(EKeys::LeftMouseButton);});
 Later(22.7f,[this,Check]{Check(KnifeAction==3,TEXT("attack_interrupts_restarted_inspect"));});
 Later(23.6f,[Tap]{Tap(EKeys::One);});
 Later(24.4f,[this,Tap]{Ammo=12;Tap(EKeys::R);});
 Later(25.6f,[Tap]{Tap(EKeys::Two);});
 Later(25.7f,[this,Check]{Check(!bReloading&&bReloadPoseHeld&&Ammo==12,TEXT("late_reload_cancel_freezes_pose_and_ammo"));});
 Later(26.5f,[this,Check,Tap]{Check(bKnife&&!bReloading&&!bReloadPoseHeld&&Ammo==12,TEXT("late_cancel_no_background_refill"));Tap(EKeys::One);});
 Later(27.3f,[this,Check,Tap]{Check(!bKnife&&!bReloading&&Ammo==12,TEXT("partial_magazine_restored_after_cancel"));Tap(EKeys::R);});
 Later(30.1f,[this,Check]{Check(!bReloading&&Ammo==30,TEXT("reload_works_after_cancel"));});
 Later(30.4f,[State]{UE_LOG(LogTemp,Display,TEXT("WEAPON_CONTROLS_AUDIT checks=%d failures=%d"),State->Checks,State->Failures);FPlatformMisc::RequestExit(false);});
}
