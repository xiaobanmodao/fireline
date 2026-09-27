#include "RangeGame.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Controller.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"

void ARangeCharacter::StartArmPoseAudit()
{
 const bool bReloadSequence=FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadElbowAudit"));
 if(!bReloadSequence&&!FParse::Param(FCommandLine::Get(),TEXT("FirelineArmPoseAudit")))return;
 if(bReloadSequence)
 {
  auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
  Later(2,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=29;});
  Later(3,[this]{Reload();});
  for(int I=0;I<=25;++I)Later(3.f+I*.1f,[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/reload-elbow-%02d.png"),I),true,false);});
  Later(6,[this]{StartAim();});
  Later(6.5,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/reload-elbow-ads.png"),true,false);});
  Later(7,[this]{EndAim();});
  Later(7.5,[this]{Ammo=29;Reload();});
  Later(8.2,[this]{Fire();});
  Later(9.5,[this]{FPlatformMisc::RequestExit(false);});
 }
 auto Samples=MakeShared<FString>(TEXT("time,knife,action,action_time,reload,side,wrist_degrees,upper_cm,forearm_cm,visible,elbow_below_shoulder_cm,downward_pole_cm,empty,reload_position\n"));
 auto LastSave=MakeShared<double>(0);
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,Samples,LastSave](UWorld* World,ELevelTick,float)
 {
  if(World!=GetWorld()||GetGameTimeSinceCreation()<1||!Arms->DoesSocketExist(TEXT("DJ_wrist_L")))return;
  for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
  {
   auto Position=[this,Side](const TCHAR* Prefix){return Arms->GetSocketTransform(*FString::Printf(TEXT("%s%s"),Prefix,Side),RTS_Component).GetLocation();};
   const FVector S=Position(TEXT("UpperArm_")),E=Position(TEXT("LowerArm_")),W=Position(TEXT("DJ_wrist_")),P=Position(TEXT("DJ_middle_01_"));
   const float Angle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct((W-E).GetSafeNormal(),(P-W).GetSafeNormal()),-1.f,1.f)));
   const FVector Line=(W-S).GetSafeNormal();
   const FVector Pole=(E-S)-Line*FVector::DotProduct(E-S,Line);
   *Samples+=FString::Printf(TEXT("%.5f,%d,%d,%.5f,%d,%s,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%d,%.5f\n"),GetGameTimeSinceCreation(),bKnife,KnifeAction,KnifeTime,bReloading,Side,Angle,(E-S).Size(),(W-E).Size(),!bThirdPerson&&HandLower<.05f,S.Z-E.Z,-Pole.Z,bEmptyHands,ReloadPosition);
  }
  if(GetGameTimeSinceCreation()-*LastSave>.5)
  {
   *LastSave=GetGameTimeSinceCreation();
   FFileHelper::SaveStringToFile(*Samples,*(FPaths::ProjectSavedDir()/TEXT("arm-pose-audit.csv")));
  }
 });
}
