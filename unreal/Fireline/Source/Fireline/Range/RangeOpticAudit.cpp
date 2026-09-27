#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

void ARangeCharacter::StartOpticAudit()
{
 bOpticAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineOpticAudit"));if(!bOpticAudit)return;
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 struct FState {int32 Samples=0,Failures=0;float Start=0;FTransform Mount;FString Rows=TEXT("time,red,third,knife,empty,reload,aim,view_visible,body_visible,mount_cm,mount_deg,center_cm,axis_deg\n");};
 auto State=MakeShared<FState>();State->Start=GetWorld()->GetTimeSeconds();State->Mount=ViewOptic->GetRelativeTransform();
 FString Output=FPaths::ProjectSavedDir()/TEXT("OpticAudit");FParse::Value(FCommandLine::Get(),TEXT("FirelineOpticCaptureDir="),Output);
 IFileManager::Get().MakeDirectory(*Output,true);
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Shot=[Output](FString Name){FScreenshotRequest::RequestScreenshot(Output/(Name+TEXT(".png")),true,false);};
 Later(1,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);SetCoyoteSight(true);});
 Later(2,[Shot]{Shot(TEXT("01-coyote-hip"));});
 Later(2.5f,[this]{SetCoyoteSight(false);StartAim();});
 Later(3.3f,[Shot]{Shot(TEXT("02-iron-ads"));});
 Later(4,[this]{SetCoyoteSight(true);});
 Later(5,[Shot]{Shot(TEXT("03-coyote-ads"));});
 Later(6,[this]{Controller->SetControlRotation(FRotator(35,120,0));});
 Later(7,[Shot]{Shot(TEXT("04-coyote-look-up"));});
 Later(7.5f,[this]{EndAim();ToggleView();SetActorRotation(FRotator::ZeroRotator);Controller->SetControlRotation(FRotator(-8,150,0));});
 Later(9,[Shot]{Shot(TEXT("05-third-person"));});
 Later(9.5f,[this]{Ammo=29;Reload();});
 Later(10.5f,[Shot]{Shot(TEXT("06-third-reload"));});
 Later(12.5f,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(13,[this]{Ammo=29;Reload();});
 Later(13.6f,[Shot]{Shot(TEXT("07-reload-release"));});
 Later(14.5f,[Shot]{Shot(TEXT("08-reload-insert"));});
 Later(16,[this]{EquipKnife();});
 Later(17,[Shot]{Shot(TEXT("09-knife-no-optic"));});
 Later(18,[this]{ToggleEmptyHands();});
 Later(19,[Shot]{Shot(TEXT("10-empty-no-optic"));});
 Later(20,[this]{EquipRifle();});
 Later(21,[this]{Ammo=29;Reload();});
 Later(21.5f,[this]{SetCoyoteSight(false);});
 Later(24,[this]{StartAim();});
 Later(25,[Shot]{Shot(TEXT("11-iron-after-reload-switch"));});
 Later(25.5f,[this]{SetCoyoteSight(true);});
 Later(26.5f,[this]{Fire();});
 Later(27.5f,[Shot]{Shot(TEXT("12-coyote-after-fire"));});
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,State](UWorld* World,ELevelTick,float)
 {
  if(World!=GetWorld()||World->GetTimeSeconds()-State->Start<1.5f)return;
  const float Time=World->GetTimeSeconds()-State->Start;
  const auto Actual=ViewOptic->GetComponentTransform().GetRelativeTransform(ViewGun->GetSocketTransform(TEXT("M4_body")));
  const float PositionError=FVector::Distance(Actual.GetLocation(),State->Mount.GetLocation());
  const float AngleError=FMath::RadiansToDegrees(Actual.GetRotation().AngularDistance(State->Mount.GetRotation()));
  const auto BodyMount=BodyOptic->GetComponentTransform().GetRelativeTransform(BodyGun->GetSocketTransform(TEXT("M4_body")));
  const FTransform Sight=ViewOptic->GetSocketTransform(TEXT("SightCenter")).GetRelativeTransform(Camera->GetComponentTransform());
  const float CenterError=FVector2D(Sight.GetLocation().Y,Sight.GetLocation().Z).Size();
  const float AxisError=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Sight.GetUnitAxis(EAxis::X).X,-1.,1.)));
  bool Pass=HasCoyoteSight()&&PositionError<.02f&&AngleError<.02f;
  Pass&=FVector::Distance(BodyMount.GetLocation(),State->Mount.GetLocation())<.02f&&FMath::RadiansToDegrees(BodyMount.GetRotation().AngularDistance(State->Mount.GetRotation()))<.02f;
  Pass&=ViewOptic->IsVisible()==(UsesCoyoteSight()&&ViewGun->IsVisible());
  Pass&=BodyOptic->IsVisible()==(UsesCoyoteSight()&&BodyGun->IsVisible());
  if(UsesCoyoteSight()&&!bThirdPerson&&AimAlpha>.999f&&SightChangeLeft<=0&&Time<7.4f)Pass&=CenterError<.02f&&AxisError<.02f;
  ++State->Samples;State->Failures+=!Pass;
  if(!Pass&&State->Failures<6)UE_LOG(LogTemp,Warning,TEXT("OPTIC_AUDIT_FAIL t=%.2f mount=%.5f/%.5f center=%.5f axis=%.5f"),Time,PositionError,AngleError,CenterError,AxisError);
  State->Rows+=FString::Printf(TEXT("%.4f,%d,%d,%d,%d,%d,%.4f,%d,%d,%.6f,%.6f,%.6f,%.6f\n"),Time,UsesCoyoteSight(),bThirdPerson,bKnife,bEmptyHands,bReloading,AimAlpha,ViewOptic->IsVisible(),BodyOptic->IsVisible(),PositionError,AngleError,CenterError,AxisError);
 });
 Later(29,[State,Output]{FFileHelper::SaveStringToFile(State->Rows,*(Output/TEXT("optic-audit.csv")));UE_LOG(LogTemp,Display,TEXT("OPTIC_AUDIT samples=%d failures=%d"),State->Samples,State->Failures);FPlatformMisc::RequestExit(false);});
}
