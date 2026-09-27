#include "RangeGame.h"
#include "RangeAudio.h"
#include "AudioMixerBlueprintLibrary.h"
#include "RangeBallistics.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void ARangeCharacter::StartShootingAudit()
{
 bShootingAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineShootingAudit"));if(!bShootingAudit)return;
 // This audit calls gameplay methods directly. Ignore physical key presses so
 // an incidental R while the window is focused cannot invalidate damage/audio cases.
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 ShootingAuditSamples=TEXT("time,event,health,shots,hits,kills,headshots,ammo,recoil_pitch,recoil_yaw\n");
 float D=0;int Passed=0;
 Passed+=RangeRayCapsule(FVector(-10,0,0),FVector(1,0,0),20,FVector(0,0,-2),FVector(0,0,2),1,D)&&FMath::Abs(D-9)<.001;
 Passed+=!RangeRayCapsule(FVector(-10,2,0),FVector(1,0,0),20,FVector(0,0,-2),FVector(0,0,2),1,D);
 Passed+=RangeRayCapsule(FVector(0,0,-10),FVector(0,0,1),20,FVector(0,0,-2),FVector(0,0,2),1,D)&&FMath::Abs(D-7)<.001;
 Passed+=!RangeRayCapsule(FVector(-10,0,0),FVector(1,0,0),8,FVector(0,0,-2),FVector(0,0,2),1,D);
 Passed+=RangeRayCapsule(FVector::ZeroVector,FVector(1,0,0),20,FVector::ZeroVector,FVector::ZeroVector,1,D)&&D==0;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_BALLISTICS_TESTS %d/5"),Passed);
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineAudioCapture")))
 {
  Later(.5f,[this]{UAudioMixerBlueprintLibrary::StartRecordingOutput(this,16);});
  Later(1.2f,[this]{
   const FVector Ear=Camera->GetComponentLocation(),Forward=Camera->GetForwardVector(),Right=Camera->GetRightVector();
   const FVector Start=Ear-Forward*500+Right*100,End=Ear+Forward*500+Right*100;
   const bool OwnRejected=!CombatAudio->Flyby(Start,End,this);
   const bool DistantRejected=!CombatAudio->Flyby(Start+Right*500,End+Right*500,nullptr);
   const bool Valid=CombatAudio->Flyby(Start,End,nullptr);
   const bool Cooldown=!CombatAudio->Flyby(Start,End,nullptr);
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_AUDIO_FLYBY own=%d distant=%d valid=%d cooldown=%d"),OwnRejected,DistantRejected,Valid,Cooldown);
  });
  Later(1.8f,[this]{const FVector Ear=Camera->GetComponentLocation(),Forward=Camera->GetForwardVector(),Right=Camera->GetRightVector();CombatAudio->Flyby(Ear-Forward*500-Right*100,Ear+Forward*500-Right*100,nullptr);});
  Later(15.5f,[this]{
   UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("audio-gameplay"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));
   const FString Report=FString::Printf(TEXT("{\"loaded\":%d,\"shots\":%d,\"gun_sounds\":%d,\"hits\":%d,\"body_sounds\":%d,\"headshots\":%d,\"head_sounds\":%d,\"cover_sounds\":%d,\"reload_sounds\":%d,\"flyby_sounds\":%d}"),CombatAudio->LoadedCount(),Shots,CombatAudio->ShotCount,Hits,CombatAudio->HitCount,Headshots,CombatAudio->HeadCount,CombatAudio->CoverCount,CombatAudio->ReloadCount,CombatAudio->FlybyCount);
   FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("audio-audit.json")));
  });
 }
 auto Target=[this]()->ARangeTarget*{TActorIterator<ARangeTarget> It(GetWorld());return It?*It:nullptr;};
 auto Record=[this,Target](const TCHAR* Event){auto* T=Target();ShootingAuditSamples+=FString::Printf(TEXT("%.4f,%s,%d,%d,%d,%d,%d,%d,%.6f,%.6f\n"),GetGameTimeSinceCreation(),Event,T?T->GetHealth():-1,Shots,Hits,Kills,Headshots,Ammo,RecoilPitch,RecoilYaw);};
 Later(2,[this,Target]{if(auto* T=Target())SetActorLocation(T->GetActorLocation()+T->GetActorForwardVector()*450+FVector(0,0,5));});
 Later(3,[this,Target,Record]{
  auto* T=Target();if(!T)return;
  auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>();Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Wall->SetActorLocation((Camera->GetComponentLocation()+T->Body->GetSocketLocation(TEXT("Torso")))*.5f);
  Wall->SetActorScale3D(FVector(.5,1.2,1.6));Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
  Fire();Record(TEXT("cover"));Wall->Destroy();
 });
 Later(4,[this,Record]{Fire();Fire();Record(TEXT("body1_cooldown"));FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/shooting-flash.png"),true,false);});
 Later(4.3f,[this,Record]{Fire();Record(TEXT("body2"));});
 Later(4.6f,[this,Record]{Fire();Record(TEXT("body3"));});
 Later(6.8f,[this,Target,Record]{
  auto* T=Target();if(!T)return;
  const FVector End=T->Body->GetSocketLocation(TEXT("Head"))+FVector(0,0,6);
  const FVector Muzzle=ViewGun->GetSocketLocation(TEXT("M4_Flash"));
  auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>();Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Wall->SetActorLocation(Muzzle+(End-Muzzle).GetSafeNormal()*12);Wall->SetActorScale3D(FVector(.08));
  Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
  Fire();Record(TEXT("muzzle_cover"));Wall->Destroy();
 });
 Later(7,[this,Record]{Fire();Record(TEXT("head"));});
 Later(11.5f,[Record]{Record(TEXT("settled"));});
 Later(12,[this,Record]{Reload();const int Before=Shots;Fire();Record(Shots==Before&&!bReloading&&bFireAfterReloadReturn?TEXT("reload_interrupted"):TEXT("reload_failed"));});
 const TArray<float> Times={4.025f,4.65f,7.035f,8.65f,9.05f,13.15f,15.4f};
 for(int I=0;I<Times.Num();++I)Later(Times[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/shooting-%02d.png"),I),true,false);});
 Later(16,[this,Record]{Record(TEXT("complete"));FFileHelper::SaveStringToFile(ShootingAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("shooting-audit.csv")));FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::TickShootingAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();TActorIterator<ARangeTarget> It(GetWorld());if(!It)return;
 if(T>2&&T<8)
 {
  // Keep aiming at the same body region; reset recoil only for the controlled
  // damage cases. The following burst uses the real unmodified recoil path.
  const FVector Aim=T<6?It->Body->GetSocketLocation(TEXT("Torso")):It->Body->GetSocketLocation(TEXT("Head"))+FVector(0,0,6);
  Controller->SetControlRotation((Aim-Camera->GetComponentLocation()).Rotation());
  RecoilPitch=RecoilYaw=RecoilPitchVelocity=RecoilYawVelocity=0;Camera->SetRelativeRotation(FRotator::ZeroRotator);
 }
 if(T>=8&&T<12){bAimHeld=true;Controller->SetControlRotation(FRotator(8,90,0));}
 if(T>=8.2&&T<9.4&&FireCooldown<=0)Fire();
 if(T>=8.2&&T<11.5)ShootingAuditSamples+=FString::Printf(TEXT("%.4f,recoil,%d,%d,%d,%d,%d,%d,%.6f,%.6f\n"),T,It->GetHealth(),Shots,Hits,Kills,Headshots,Ammo,RecoilPitch,RecoilYaw);
}
