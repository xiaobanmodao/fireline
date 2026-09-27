#include "RangeGame.h"
#include "RangePlayerController.h"
#include "RangeCasingFX.h"
#include "RangeWeaponTiming.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"

void ARangeCharacter::StartReloadCasingAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadCasingAudit")))return;
 // Screenshot readback must not skip the before/after insertion samples.
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);
 struct FState{int Checks=0,Failures=0,Seatings=0,Peak=0,LastAmmo=30;bool WasReloading=false;float LastPosition=0;FString Text,Rows=TEXT("time,slot,ammo,reloading,position,committed,shots,casings,active\n");};
 auto S=MakeShared<FState>();const FString Output=FPaths::ProjectSavedDir()/TEXT("ReloadCasingAudit");IFileManager::Get().MakeDirectory(*Output,true);
 auto Check=[S](bool Pass,FString Name){++S->Checks;S->Failures+=!Pass;S->Text+=Name+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("RELOAD_CASING_CHECK %s pass=%d"),*Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Shot=[Output](const TCHAR* Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadCasingCapture")))FScreenshotRequest::RequestScreenshot(Output/(FString(Name)+TEXT(".png")),true,false);};
 auto Trigger=[this](bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 Later(1,[this,Check]{Controller->SetControlRotation(FRotator::ZeroRotator);Check(CasingFX&&CasingFX->AssetsReady(),TEXT("both spent-case meshes loaded"));Ammo=12;Reload();});
 Later(1.f+RangeWeaponTiming::MagazineSeat(false,false)/RangeWeaponTiming::ReloadPlayRate(false)-.08f,[this,Check,Shot]{Check(bReloading&&Ammo==12&&!bReloadAmmoCommitted,TEXT("M4 before seating keeps old ammo"));Shot(TEXT("01-m4-before-seat"));});
 Later(1.f+RangeWeaponTiming::MagazineSeat(false,false)/RangeWeaponTiming::ReloadPlayRate(false)+.07f,[this,Check,Shot]{Check(bReloading&&Ammo==30&&bReloadAmmoCommitted,TEXT("M4 fills at insertion before bolt recovery"));Shot(TEXT("02-m4-seated"));});
 Later(2.48f,[this]{Fire();});Later(2.80f,[this,Check]{Check(!bReloading&&Ammo==29&&Shots==1,TEXT("M4 fires after seating interruption"));});
 Later(3.8f,[this,Check]{Check(Ammo==29,TEXT("M4 cancelled tail never refills twice"));Ammo=12;Reload();});
 Later(4.4f,[this]{Fire();});Later(4.8f,[this,Check]{Check(!bReloading&&Ammo==11,TEXT("M4 early interruption preserves old ammunition"));Ammo=12;Reload();});
 Later(5.3f,[this]{EquipKnife();});Later(6.1f,[this,Check]{Check(bKnife&&RifleAmmo==12,TEXT("M4 switch before seating keeps old magazine"));EquipRifle();});
 Later(7,[this]{Ammo=0;Reload();});Later(8.45f,[this,Check]{Check(bReloading&&Ammo==30&&bEmptyReload,TEXT("M4 empty reload commits before animation ends"));Fire();});
 Later(8.8f,[this,Check]{Check(Ammo==29&&!bReloading,TEXT("M4 empty reload can fire after insertion"));Ammo=8;Reload();});
 Later(10.2f,[this]{EquipKnife();});Later(11,[this,Check]{Check(bKnife&&RifleAmmo==30,TEXT("M4 seated magazine survives switch"));EquipRifle();});
 Later(12,[this,Trigger]{Controller->SetControlRotation(FRotator::ZeroRotator);Trigger(true);});
 Later(12.16f,[Shot]{Shot(TEXT("03-m4-hip-casings"));});Later(12.55f,[this]{StartAim();});
 Later(12.97f,[Shot]{Shot(TEXT("04-m4-aim-casings"));});Later(13.2f,[Trigger]{Trigger(false);});Later(13.4f,[this]{EndAim();ToggleView();});
 Later(14,[Trigger]{Trigger(true);});Later(14.16f,[Shot]{Shot(TEXT("05-m4-third-casings"));});Later(14.4f,[Trigger]{Trigger(false);});
 Later(15,[this]{ToggleView();EquipMP7();});Later(16.3f,[this,Check]{Check(bMP7,TEXT("MP7 equipped"));Ammo=14;Reload();});
 Later(17.63f,[this,Check,Shot]{Check(bReloading&&Ammo==14&&!bReloadAmmoCommitted,TEXT("MP7 normal before seating keeps old ammo"));Shot(TEXT("06-mp7-before-seat"));});
 Later(17.80f,[this,Check,Shot]{Check(bReloading&&Ammo==40&&bReloadAmmoCommitted,TEXT("MP7 normal fills at magazine seating"));Shot(TEXT("07-mp7-seated"));});
 Later(17.95f,[this]{Fire();});Later(18.3f,[this,Check]{Check(!bReloading&&Ammo==39,TEXT("MP7 normal fire interruption retains new magazine"));});
 Later(19,[this,Check]{Check(Ammo==39,TEXT("MP7 cancelled tail never refills twice"));Ammo=0;Reload();FApp::SetFixedDeltaTime(1.0/15.0);});
 Later(20.02f,[this,Check]{Check(Ammo==0&&bReloading,TEXT("MP7 empty before seating remains empty"));});
 Later(20.20f,[this,Check]{Check(Ammo==40&&bReloading&&bEmptyReload,TEXT("MP7 empty fills at earlier seating frame"));Fire();});
 Later(20.6f,[this,Check]{Check(!bReloading&&Ammo==39,TEXT("MP7 empty interruption can fire new magazine at 15 FPS"));FApp::SetFixedDeltaTime(1.0/60.0);Ammo=10;Reload();});
 Later(21.2f,[this]{Fire();});Later(21.6f,[this,Check]{Check(Ammo==9&&!bReloading,TEXT("MP7 early fire interruption adds no ammo"));Ammo=10;Reload();});
 Later(22.1f,[this]{EquipKnife();});Later(23,[this,Check]{Check(bKnife&&MP7Ammo==10,TEXT("MP7 early switch keeps old magazine"));EquipMP7();});
 Later(24.3f,[this]{Ammo=1;Fire();});Later(24.55f,[this,Check]{Check(bReloading&&Ammo==0,TEXT("last round starts automatic reload"));});
 Later(25.7f,[this,Check]{Check(bReloading&&Ammo==40&&bReloadAmmoCommitted,TEXT("automatic reload fills at insertion"));EquipRifle();});
 Later(26.5f,[this,Check]{Check(!bMP7&&MP7Ammo==40,TEXT("MP7 seated magazine survives switch"));EquipMP7();});
 Later(27.8f,[this,Check,Trigger]{Check(bMP7&&Ammo==40,TEXT("MP7 restored with filled magazine"));Controller->SetControlRotation(FRotator::ZeroRotator);Trigger(true);});
 Later(27.96f,[Shot]{Shot(TEXT("08-mp7-hip-casings"));});Later(28.25f,[this]{StartAim();});Later(28.64f,[Shot]{Shot(TEXT("09-mp7-aim-casings"));});
 Later(29.05f,[this]{EndAim();ToggleView();});Later(29.35f,[Shot]{Shot(TEXT("10-mp7-third-casings"));});
 Later(29.8f,[Trigger]{Trigger(false);});
 Later(30.1f,[this,Check,S]{Check(CasingFX->Ejected==Shots,TEXT("exactly one casing per emitted shot"));Check(S->Peak<=64&&S->Peak>=20,TEXT("sustained fire uses bounded casing pool"));});
 Later(33.2f,[this,Check,S,Output]{Check(CasingFX->ActiveCount()==0,TEXT("all casings expire after fire stops"));Check(CasingFX->Bounces>0,TEXT("casings collide and bounce on range surfaces"));Check(S->Seatings>=6,TEXT("all seating events sampled across frames"));FFileHelper::SaveStringToFile(S->Rows,*(Output/TEXT("samples.csv")));FFileHelper::SaveStringToFile(S->Text,*(Output/TEXT("checks.txt")));UE_LOG(LogTemp,Display,TEXT("RELOAD_CASING_AUDIT checks=%d failures=%d seatings=%d casings=%d peak=%d bounces=%d"),S->Checks,S->Failures,S->Seatings,CasingFX->Ejected,S->Peak,CasingFX->Bounces);FPlatformMisc::RequestExit(false);});
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S,Check](UWorld* W,ELevelTick,float)
 {
  if(W!=GetWorld())return;
  if(S->WasReloading&&bReloading&&bReloadAmmoCommitted&&S->LastAmmo!=MagazineCapacity()&&S->LastPosition<RangeWeaponTiming::MagazineSeat(bMP7,bEmptyReload))
  {++S->Seatings;Check(Ammo==MagazineCapacity(),FString::Printf(TEXT("frame crossing commits once slot %d"),WeaponSlot()));}
  S->LastAmmo=Ammo;S->LastPosition=ReloadPosition;S->WasReloading=bReloading;S->Peak=FMath::Max(S->Peak,CasingFX->ActiveCount());
  S->Rows+=FString::Printf(TEXT("%.4f,%d,%d,%d,%.4f,%d,%d,%d,%d\n"),GetGameTimeSinceCreation(),WeaponSlot(),Ammo,bReloading,ReloadPosition,bReloadAmmoCommitted,Shots,CasingFX->Ejected,CasingFX->ActiveCount());
 });
}
