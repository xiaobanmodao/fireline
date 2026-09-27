#include "RangeGame.h"
#include "RangePlayerController.h"
#include "RangeCasingFX.h"
#include "RangeShotFX.h"
#include "RangeWeaponTiming.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInstanceDynamic.h"

void ARangeCharacter::StartWeaponPolishAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponPolishAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);
 struct FState{int Checks=0,Failures=0,Peak=0;ARangeTarget* Target=nullptr;FString ChecksText,Rows=TEXT("t,ammo,shots,reload,reload_t,draw,draw_t,returning,aim,cases\n");};
 auto S=MakeShared<FState>();const FString Out=FPaths::ProjectSavedDir()/(FParse::Param(FCommandLine::Get(),TEXT("FirelineContactCarryStudy"))?TEXT("ContactCarryWeaponPolishAudit"):TEXT("WeaponPolishAudit"));IFileManager::Get().MakeDirectory(*Out,true);
 auto Check=[S](bool Pass,FString Name){++S->Checks;S->Failures+=!Pass;S->ChecksText+=Name+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("WEAPON_POLISH_CHECK %s pass=%d"),*Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Key=[this](FKey K,bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 auto Shot=[Out](const TCHAR* Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponPolishCapture")))FScreenshotRequest::RequestScreenshot(Out/(FString(Name)+TEXT(".png")),true,false);};
 auto ImmediateFire=[this,Check](FString Name){const int Before=Shots;Fire();Check(Shots==Before+1&&ReloadFireReturnLeft<=0&&!bFireAfterReloadReturn,Name);};
 Later(1,[this]{Controller->SetControlRotation(FRotator(0,30,0));EquipMP7();});
 Later(1.72f,[this,Check,Key]{const int Before=Shots;Fire();Check(IsMP7DrawLocked()&&Shots==Before,TEXT("draw still blocks fire before grip ready"));Key(EKeys::RightMouseButton,true);});
 Later(1.92f,[this,Check,ImmediateFire]{Check(MP7ActionTime<50.f/60.f&&AimAlpha>.05f&&!IsMP7DrawLocked(),TEXT("held RMB starts ADS during draw recovery"));ImmediateFire(TEXT("fire immediately during draw recovery"));});
 Later(1.98f,[Shot]{Shot(TEXT("01-draw-early-ads"));});Later(2.12f,[Key]{Key(EKeys::RightMouseButton,false);});
 Later(2.3f,[this]{EquipRifle();});Later(2.86f,[this]{EquipMP7();});
 Later(3.78f,[this,ImmediateFire,Check]{Check(MP7Action==1&&MP7ActionTime<50.f/60.f,TEXT("second draw is still visually settling"));ImmediateFire(TEXT("hip fire can cut draw tail without queued delay"));});
 Later(3.84f,[Shot]{Shot(TEXT("02-draw-early-hip"));});
 Later(4,[this]{Ammo=14;Reload();});
 Later(5.80f,[this,Check,Key]{Key(EKeys::RightMouseButton,true);Check(bReloading&&AimAlpha==0,TEXT("reload ADS waits for support hand to return"));});
 Later(5.99f,[this,Check,ImmediateFire]{Check(!bReloading&&ReloadPosition<139.f/60.f&&AimAlpha>0,TEXT("normal reload ADS preempts cosmetic tail"));ImmediateFire(TEXT("normal reload ADS fire is immediate"));});
 Later(6.06f,[Shot]{Shot(TEXT("03-normal-early-ads"));});
 Later(6.5f,[this,Key]{Key(EKeys::RightMouseButton,false);});Later(6.6f,[this]{Ammo=0;Reload();});
 Later(8.86f,[this,Check,Key]{Key(EKeys::RightMouseButton,true);Check(bReloading&&Ammo==40,TEXT("empty reload preserves insertion ammo while completing main motion"));});
 // Reload begins while the preceding ADS is lowering; include its lead-out.
 Later(9.29f,[this,Check,ImmediateFire]{Check(!bReloading&&ReloadPosition<163.f/60.f&&AimAlpha>0,TEXT("empty reload ADS preempts cosmetic tail"));ImmediateFire(TEXT("empty reload ADS fire is immediate"));});
 Later(9.35f,[Shot]{Shot(TEXT("04-empty-early-ads"));});
 Later(9.4f,[Key]{Key(EKeys::RightMouseButton,false);});Later(9.6f,[this]{Ammo=12;Reload();});
 Later(11.61f,[this,Check,ImmediateFire]{Check(IsMP7ReloadRecovery(),TEXT("normal reload has an unlocked visible tail"));ImmediateFire(TEXT("hip fire cuts normal reload tail immediately"));Check(Ammo==39&&!bReloading,TEXT("tail cancellation spends one inserted round"));});
 Later(12.4f,[this]{Ammo=0;Reload();});
 Later(14.85f,[this,Check,ImmediateFire]{Check(IsMP7ReloadRecovery(),TEXT("empty reload has an unlocked visible tail"));ImmediateFire(TEXT("hip fire cuts empty reload tail immediately"));});
 Later(15.2f,[this]{Ammo=12;Reload();});Later(15.75f,[this,Check]{const int Before=Shots;Fire();Check(Shots==Before&&bFireAfterReloadReturn,TEXT("early reload interruption retains safe grip recovery"));});
 Later(16.1f,[this,Check]{Check(Ammo==11&&!bReloading,TEXT("early reload interruption keeps old ammunition"));});
 Later(16.3f,[this,Key]{Ammo=40;Key(EKeys::LeftMouseButton,true);});Later(16.46f,[Shot]{Shot(TEXT("05-mp7-casings-hip"));});
 Later(17,[Key]{Key(EKeys::RightMouseButton,true);});Later(17.32f,[Shot]{Shot(TEXT("06-mp7-casings-ads"));});
 Later(18,[Key]{Key(EKeys::LeftMouseButton,false);Key(EKeys::RightMouseButton,false);});Later(18.3f,[this]{EquipRifle();});
 Later(19.1f,[this,Key]{Ammo=30;Key(EKeys::LeftMouseButton,true);});Later(19.26f,[Shot]{Shot(TEXT("07-m4-casings-hip"));});
 Later(19.9f,[this,Key]{SetCoyoteSight(true);Key(EKeys::RightMouseButton,true);});Later(20.26f,[Shot]{Shot(TEXT("08-m4-casings-coyote"));});
 Later(20.5f,[Key]{Key(EKeys::LeftMouseButton,false);Key(EKeys::RightMouseButton,false);});
 Later(21,[this,S]{for(TActorIterator<ARangeTarget> It(GetWorld());It;++It){if(!S->Target||FMath::Abs(It->GetActorLocation().Y)<FMath::Abs(S->Target->GetActorLocation().Y))S->Target=*It;}check(S->Target);SetActorLocation(S->Target->GetActorLocation()+S->Target->GetActorForwardVector()*600+FVector(0,0,5));});
 Later(21.10f,[this,S]{Controller->SetControlRotation((S->Target->Body->GetSocketLocation(TEXT("Chest"))-Camera->GetComponentLocation()).Rotation());});
 Later(21.15f,[this]{Fire();});
 Later(21.22f,[this,Check,Shot]{Check(bLastShieldHit&&!bLastShieldBreak,TEXT("actual shield hit uses enlarged effect"));Shot(TEXT("09-shield-hit"));});
 Later(21.6f,[S]{S->Target->RegisterHit(FMath::RoundToInt(S->Target->GetShield()-10));});Later(21.75f,[this]{Fire();});
 Later(21.82f,[this,Check,Shot]{Check(bLastShieldBreak,TEXT("actual shield break uses enlarged fragments"));Shot(TEXT("10-shield-break"));});
 Later(22.2f,[this]{Fire();});
 Later(22.27f,[this,Check,Shot]{bool Found=false;for(TActorIterator<ARangeShotFX> It(GetWorld());It;++It)if(It->HasBloodSprite())for(auto* C:It->GetComponents())if(auto* M=Cast<UStaticMeshComponent>(C))if(M->GetName()==TEXT("BloodMistSprite"))Found|=M->GetComponentScale().GetMax()>.7f;Check(Found&&!bLastShieldHit,TEXT("actual flesh hit has larger blood sprite"));Shot(TEXT("11-flesh-hit"));});
 Later(22.39f,[Shot]{Shot(TEXT("12-flesh-middle"));});
 Later(22.8f,[this]{Fire();});
 Later(22.87f,[Shot]{Shot(TEXT("12b-flesh-second-hit"));});
 Later(23,[this]{Controller->SetControlRotation(FRotator(-40,30,0));Fire();});Later(23.07f,[Shot]{Shot(TEXT("13-cover-hit"));});
 Later(23.6f,[this,Check,S]{Check(CasingFX->Ejected==Shots,TEXT("one case per shot across all tail interruptions"));Check(S->Peak<=64,TEXT("bounded casing pool under sustained fire"));Check(CasingFX->Bounces>0,TEXT("new capsule collision reaches surfaces"));});
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineImpactProbe")))
 {
  Later(24,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
  Later(24.2f,[this]{for(int I=0;I<2;++I){
   const FVector Point=Camera->GetComponentLocation()+Camera->GetForwardVector()*180+Camera->GetRightVector()*(I?55:-55);
   auto* FX=GetWorld()->SpawnActor<ARangeShotFX>();FX->Setup(ViewGun,Point,-Camera->GetForwardVector(),true,true);FX->SetFleshImpact();FX->SetActorTickEnabled(false);FX->SetLifeSpan(5);
   for(auto* C:FX->GetComponents())if(auto* M=Cast<UStaticMeshComponent>(C))if(M->GetName()==TEXT("BloodMistSprite")){
    auto* Mat=CastChecked<UMaterialInstanceDynamic>(M->GetMaterial(0));Mat->SetScalarParameterValue(TEXT("Frame"),3);Mat->SetScalarParameterValue(TEXT("Fade"),1);
    if(I){M->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_ShotFX.M_ShotFX")));M->CreateAndSetMaterialInstanceDynamic(0)->SetVectorParameterValue(TEXT("Color"),FLinearColor(1,0,0));}
    UE_LOG(LogTemp,Display,TEXT("BLOOD_PROBE index=%d pos=%s rotation=%s scale=%s bounds=%s mesh=%s material=%s"),I,*M->GetComponentLocation().ToString(),*M->GetComponentRotation().ToString(),*M->GetComponentScale().ToString(),*M->Bounds.BoxExtent.ToString(),*GetNameSafe(M->GetStaticMesh()),*Mat->GetFullName());
   }
  }});
  Later(25,[Shot]{Shot(TEXT("14-blood-material-probe"));});
 }
 Later(26.5f,[this,Check,S,Out]{Check(CasingFX->ActiveCount()==0,TEXT("casings settle and expire after firing"));FFileHelper::SaveStringToFile(S->ChecksText,*(Out/TEXT("checks.txt")));FFileHelper::SaveStringToFile(S->Rows,*(Out/TEXT("samples.csv")));UE_LOG(LogTemp,Display,TEXT("WEAPON_POLISH_AUDIT checks=%d failures=%d shots=%d peak=%d contacts=%d"),S->Checks,S->Failures,Shots,S->Peak,CasingFX->Bounces);FPlatformMisc::RequestExit(false);});
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S](UWorld* W,ELevelTick,float){if(W!=GetWorld())return;S->Peak=FMath::Max(S->Peak,CasingFX->ActiveCount());S->Rows+=FString::Printf(TEXT("%.4f,%d,%d,%d,%.4f,%d,%.4f,%d,%.4f,%d\n"),GetGameTimeSinceCreation(),Ammo,Shots,bReloading,ReloadPosition,MP7Action,MP7ActionTime,bMP7ActionReturning,AimAlpha,CasingFX->ActiveCount());});
}
