#include "RangeGame.h"
#include "RangePlayerController.h"
#include "RangeWeaponTiming.h"
#include "RangeCasingFX.h"
#include "RangeAudio.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"

void ARangeCharacter::StartM4AnimationAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineM4AnimationAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);
 struct FState{int Checks=0,Failures=0,Samples=0,PoseFailures=0,AudioBefore=0;float TacticalBolt=0,EmptyBolt=0;float MaxWrist=0,MinElbow=100,MaxGripError=0;FString Text,Rows=TEXT("time,third_person,slot,action,action_time,reload,reload_time,ammo,aim,wrist_l,wrist_r,elbow_l,elbow_r,grip_error_cm\n");};
 auto S=MakeShared<FState>();const FString Out=FPaths::ProjectSavedDir()/(FParse::Param(FCommandLine::Get(),TEXT("FirelineContactCarryStudy"))?TEXT("ContactCarryM4AnimationAudit"):TEXT("M4AnimationAudit"));IFileManager::Get().MakeDirectory(*Out,true);
 auto Check=[S](bool Pass,const TCHAR* Name){++S->Checks;S->Failures+=!Pass;S->Text+=FString(Name)+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("M4_ANIMATION_CHECK %s pass=%d"),Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Key=[this](FKey K,bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 auto Capture=[Out](FString Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineM4AnimationCapture")))FScreenshotRequest::RequestScreenshot(Out/(Name+TEXT(".png")),true,false);};
 Later(1,[this,Check,Capture]{if(bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);for(const TCHAR* N:{TEXT("Draw"),TEXT("Remove"),TEXT("Reload"),TEXT("TacticalReload")})Check(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/SycgffM4/A_M4_%s.A_M4_%s"),N,N))!=nullptr,*FString::Printf(TEXT("selected %s asset loaded"),N));Capture(TEXT("01-hold"));});
 Later(1.3f,[this]{EquipKnife();});Later(1.55f,[Capture]{Capture(TEXT("02-remove"));});
 Later(2.15f,[this,Check]{Check(bKnife&&HandSwitchLeft<=0,TEXT("full M4 remove switches to knife"));});
 Later(2.3f,[this]{EquipRifle();});Later(2.57f,[Capture]{Capture(TEXT("03-draw-first"));});
 Later(2.65f,[this,Check]{const int Before=Shots;Fire();Check(Shots==Before&&IsM4DrawLocked(),TEXT("draw blocks firing before grip ready"));StartAim();});
 Later(2.72f,[Capture]{Capture(TEXT("04-draw-middle"));});
 Later(3.04f,[this,Check,Capture]{Check(AimAlpha>0&&!IsM4DrawLocked(),TEXT("held ADS starts during draw cosmetic tail"));Capture(TEXT("05-draw-ready-ads"));});
 Later(3.4f,[this]{EndAim();});Later(3.7f,[this,S,Check]{Ammo=12;S->AudioBefore=CombatAudio->ReloadCount;Reload();Check(!bEmptyReload&&FMath::IsNearlyEqual(ReloadDuration,84.f/60.f),TEXT("loaded magazine selects shorter tactical timeline"));});
 for(int I=0;I<20;++I)Later(3.72f+I*.1f,[Capture,I]{Capture(FString::Printf(TEXT("reload-fp-%02d"),I));});
 Later(4.05f,[this,Check]{Check(Arms->IsBoneHiddenByName(TEXT("M4_mag"))&&GetMesh()->IsBoneHiddenByName(TEXT("M4_mag")),TEXT("dropped magazine has no animated duplicate"));});
 Later(4.35f,[this,Check]{Check(!Arms->IsBoneHiddenByName(TEXT("M4_mag"))&&!GetMesh()->IsBoneHiddenByName(TEXT("M4_mag")),TEXT("replacement magazine visible in hand before insertion"));});
 Later(4.53f,[this,Check]{Check(Ammo==12&&bReloading&&!bReloadAmmoCommitted,TEXT("before new insertion frame retains old ammo"));});
 Later(4.86f,[this,Check]{Check(Ammo==30&&bReloading&&bReloadAmmoCommitted,TEXT("tactical insertion commits ammo before support-grip return"));});
 Later(5.31f,[this,S,Check]{Check(bReloading&&IsM4ReloadRecovery(),TEXT("tactical ready gate occurs inside cosmetic tail"));Check(CombatAudio->ReloadCount-S->AudioBefore==2,TEXT("tactical reload plays magazine sounds only"));const int Before=Shots;Fire();Check(Shots==Before+1&&!bReloading&&Ammo==29,TEXT("ready reload tail accepts immediate fire"));});
 Later(6.2f,[this]{Ammo=8;Reload();});Later(6.6f,[this]{Fire();});
 Later(6.85f,[this,Check]{Check(Ammo==7&&!bReloading,TEXT("early reload interruption retains old ammo"));});
 Later(7,[this]{Ammo=6;Reload();});Later(8.14f,[this]{Fire();});
 Later(8.37f,[this,Check]{Check(Ammo==29&&!bReloading,TEXT("after-seat interruption retains new ammo"));});
 Later(9,[this]{Ammo=6;Reload();});Later(9.4f,[this]{EquipKnife();});
 Later(10.25f,[this,Check]{Check(bKnife&&RifleAmmo==6,TEXT("switch during reload cancels unseated refill"));});
 Later(10.4f,[this]{EquipRifle();});Later(11.3f,[this]{Ammo=0;Reload();});
 Later(12.47f,[this,Check]{Check(bEmptyReload&&bReloadAmmoCommitted&&Ammo==30,TEXT("empty reload refills at same insertion frame"));Fire();});
 Later(12.75f,[this,Check]{Check(!bReloading&&Ammo==29,TEXT("empty reload accepts fire interruption"));});
 Later(13,[this]{StartCrouch();Ammo=12;Reload();});Later(13.85f,[Capture]{Capture(TEXT("06-crouch-insert"));});Later(14.37f,[Capture]{Capture(TEXT("07-crouch-return"));});
 Later(15,[this]{EndCrouch();if(!bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator(-8,145,0));});Later(15.6f,[this]{Ammo=12;Reload();});
 for(int I=0;I<20;++I)Later(15.62f+I*.1f,[Capture,I]{Capture(FString::Printf(TEXT("reload-body-%02d"),I));});
 Later(17.7f,[this,Check]{Check(bThirdPerson&&!bReloading&&Ammo==30,TEXT("third-person full reload completes"));if(bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(18,[Key]{Key(EKeys::W,true);Key(EKeys::LeftMouseButton,true);});Later(18.12f,[Capture]{Capture(TEXT("08-moving-burst"));});
 Later(19,[Key]{Key(EKeys::LeftMouseButton,false);Key(EKeys::W,false);});
 Later(19.5f,[this]{StartAim();SetCoyoteSight(true);});Later(20.1f,[Capture]{Capture(TEXT("09-coyote"));});Later(20.4f,[this]{EndAim();EquipMP7();});
 Later(21.7f,[this,Check,Capture]{Check(bMP7&&!IsMP7DrawLocked(),TEXT("MP7 original draw preserved"));Capture(TEXT("10-mp7"));});
 Later(22,[this]{Ammo=20;Reload();});Later(23.96f,[this,Check]{const int Before=Shots;Fire();Check(Shots==Before+1&&Ammo==39&&!bReloading,TEXT("MP7 original reload readiness preserved"));});
 Later(24.4f,[this]{EquipRifle();});Later(25.5f,[this,S]{S->AudioBefore=CombatAudio->ReloadCount;Ammo=1;Fire();});
 Later(25.85f,[this,Check]{Check(Ammo==0&&bReloading,TEXT("last round starts automatic reload"));});
 Later(26.75f,[this,Check]{Check(Ammo==30&&bReloadAmmoCommitted,TEXT("automatic reload commits new seat event"));});
 Later(27.94f,[this,S,Check]{Check(bEmptyReload&&FMath::IsNearlyEqual(ReloadDuration,110.f/60.f),TEXT("empty magazine retains complete reload timeline"));Check(CombatAudio->ReloadCount-S->AudioBefore==4,TEXT("empty reload retains both bolt sounds"));Check(Ammo==30&&!bReloading,TEXT("automatic reload completes once"));CycleWeapon();CycleWeapon();CycleWeaponBack();});
 Later(30,[this,Check]{Check(bMP7&&HandSwitchLeft<=0&&QueuedWeapon<0,TEXT("rapid wheel resolves queued selection"));Check(CasingFX->Ejected==Shots,TEXT("one spent case per successful shot retained"));});
 // Sample the evaluated visible mesh, not just authored matrices. Transitions
 // are retained in the CSV; pose limits include their actual blended result.
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S](UWorld* W,ELevelTick,float)
 {
  if(W!=GetWorld()||WeaponSlot()!=1||GetGameTimeSinceCreation()<.8f)return;
  auto* M=bThirdPerson?GetMesh():Arms.Get();float Wrist[2],Below[2];int I=0;
  for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
  {
   auto P=[M,Side](const TCHAR* N){return M->GetSocketTransform(*FString::Printf(TEXT("%s_%s"),N,Side),RTS_Component).GetLocation();};
   const FVector A=P(TEXT("UpperArm")),E=P(TEXT("LowerArm")),H=P(TEXT("DJ_wrist")),F=P(TEXT("DJ_middle_01"));
   Wrist[I]=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((H-E).GetSafeNormal().Dot((F-H).GetSafeNormal()),-1.,1.)));Below[I]=A.Z-E.Z;
   S->MaxWrist=FMath::Max(S->MaxWrist,Wrist[I]);S->MinElbow=FMath::Min(S->MinElbow,Below[I]);++I;
  }
  if(bReloading&&ReloadPosition>55.f/60.f)
  {
   const FVector Bolt=M->GetSocketTransform(TEXT("M4_charginghandle"),RTS_Component).GetLocation();
   const FTransform Receiver=M->GetSocketTransform(TEXT("M4_body"),RTS_Component);
   static FVector RestBolt=Receiver.InverseTransformPosition(Bolt);
   // Imported rig carries unit scale; measure component-space centimetres.
   const float Travel=Receiver.TransformVector(Receiver.InverseTransformPosition(Bolt)-RestBolt).Size();
   if(bEmptyReload)S->EmptyBolt=FMath::Max(S->EmptyBolt,Travel);else S->TacticalBolt=FMath::Max(S->TacticalBolt,Travel);
  }
  const FTransform Gun=M->GetSocketTransform(TEXT("M4_body"),RTS_Component);const FVector Hand=Gun.InverseTransformPosition(M->GetSocketTransform(TEXT("DJ_index_03_R"),RTS_Component).GetLocation());
  static FVector Baseline=Hand;const float Error=(Hand-Baseline).Size();S->MaxGripError=FMath::Max(S->MaxGripError,Error);++S->Samples;
  if(Wrist[0]>42||Wrist[1]>42||Below[0]<0||Below[1]<0||Error>.7f)++S->PoseFailures;
  S->Rows+=FString::Printf(TEXT("%.4f,%d,%d,%d,%.4f,%d,%.4f,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.5f\n"),GetGameTimeSinceCreation(),bThirdPerson,WeaponSlot(),M4Action,M4Time,bReloading,ReloadPosition,Ammo,AimAlpha,Wrist[0],Wrist[1],Below[0],Below[1],Error);
 });
 Later(30.5f,[S,Out,Check]{Check(S->TacticalBolt<.05f,TEXT("evaluated tactical charging handle stays closed"));Check(S->EmptyBolt>1.f,TEXT("evaluated empty reload still operates charging handle"));Check(S->Samples>500&&S->PoseFailures==0,TEXT("evaluated arms stay bounded and trigger grip remains registered"));FFileHelper::SaveStringToFile(S->Rows,*(Out/TEXT("samples.csv")));FFileHelper::SaveStringToFile(S->Text,*(Out/TEXT("checks.txt")));UE_LOG(LogTemp,Display,TEXT("M4_ANIMATION_AUDIT checks=%d failures=%d pose_samples=%d pose_failures=%d max_wrist=%.3f min_elbow=%.3f max_grip_local=%.4f tactical_bolt_cm=%.4f empty_bolt_cm=%.4f"),S->Checks,S->Failures,S->Samples,S->PoseFailures,S->MaxWrist,S->MinElbow,S->MaxGripError,S->TacticalBolt,S->EmptyBolt);FPlatformMisc::RequestExit(false);});
}
