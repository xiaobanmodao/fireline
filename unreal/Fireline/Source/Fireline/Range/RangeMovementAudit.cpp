#include "RangeGame.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void ARangeCharacter::StartMovementAudit()
{
 bMovementAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementAudit"));
 if(!bMovementAudit)return;
 MovementAuditSamples=TEXT("time,dt,phase,actor_x,actor_y,speed,root_x,root_y,pelvis_x,pelvis_y,head_x,head_y\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 Later(2,[this]{
  SetActorLocation(FVector(-500,-750,95));GetCharacterMovement()->StopMovementImmediately();
  ToggleView();Boom->TargetArmLength=300;Controller->SetControlRotation(FRotator(-12,-50,0));
 });
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementCapture")))
 {
  const TArray<float> Times={3.8f,4.05f,4.3f,6.8f,8.2f,10.8f,11.05f,11.3f,13.8f,16.5f,19.f};
  for(int I=0;I<Times.Num();++I)Later(Times[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/movement-%02d.png"),I),true,false);});
 }
 Later(20,[this]{
  FFileHelper::SaveStringToFile(MovementAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("movement-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_MOVEMENT_AUDIT complete"));FPlatformMisc::RequestExit(false);
 });
}
void ARangeCharacter::TickMovementAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();if(T<3||T>=20)return;
 const bool Run=(T<6)||(T>=14&&T<17);
 const bool Sprint=(T>=7&&T<9)||(T>=10&&T<13);
 const float Direction=(T>=7&&T<9)||(T>=14&&T<17)?-1.f:1.f;
 GetCharacterMovement()->MaxWalkSpeed=Sprint?700:450;
 if(Run||Sprint)AddMovementInput(FVector(Direction,0,0));
 const FVector P=GetActorLocation();
 const FVector Root=GetMesh()->GetSocketTransform(TEXT("Root"),RTS_Component).GetLocation();
 const FVector Pelvis=GetMesh()->GetSocketTransform(TEXT("Pelvis"),RTS_Component).GetLocation();
 const FVector Head=GetMesh()->GetSocketTransform(TEXT("Head"),RTS_Component).GetLocation();
 MovementAuditSamples+=FString::Printf(TEXT("%.6f,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),T,Dt,Sprint?TEXT("sprint"):Run?TEXT("run"):TEXT("stop"),P.X,P.Y,GetVelocity().Size2D(),Root.X,Root.Y,Pelvis.X,Pelvis.Y,Head.X,Head.Y);
 // The gold capsule is the actual collision/movement position, not the animated mesh.
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementCapture")))DrawDebugCapsule(GetWorld(),P,90,32,FQuat::Identity,FColor(255,190,65),false,0,0,.7f);
}
