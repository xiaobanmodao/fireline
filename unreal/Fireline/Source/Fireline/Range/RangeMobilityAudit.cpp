#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "TimerManager.h"

void ARangeCharacter::StartMobilityAudit()
{
 bMobilityAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineMobilityAudit"));if(!bMobilityAudit)return;
 float FPS=60.f;FParse::Value(FCommandLine::Get(),TEXT("FirelineMobilityFPS="),FPS);
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/FMath::Clamp(FPS,15.f,240.f));
 MobilityAuditSamples=TEXT("time,x,y,z,speed,vz,crouched,sliding,slide_blend,air_blend,land_time,camera_z,head_z,foot_l_z,foot_r_z,root_x,root_y,reloading,ammo,local_forward,local_right,direction,jump_serial,landing_serial,slide_serial,slide_entry,landing_speed,phase\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Key=[this](FKey K,EInputEvent E){CastChecked<APlayerController>(Controller)->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
 Later(2,[this]{SetActorLocation(FVector(-450,-350,95));Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(3,[Key]{Key(EKeys::C,IE_Pressed);});Later(4.2f,[Key]{Key(EKeys::C,IE_Released);});
 Later(5,[this]{ToggleView();Boom->TargetArmLength=300;Controller->SetControlRotation(FRotator(-10,50,0));});
 Later(5.5f,[Key]{Key(EKeys::C,IE_Pressed);});Later(7.2f,[Key]{Key(EKeys::C,IE_Released);});
 Later(5.6f,[this]{Controller->SetControlRotation(FRotator(-10,145,0));});Later(6,[this]{Controller->SetControlRotation(FRotator(-10,50,0));});
 Later(7.4f,[this]{Controller->SetControlRotation(FRotator(-10,95,0));});Later(7.9f,[this]{Controller->SetControlRotation(FRotator(-10,50,0));});
 Later(8,[Key]{Key(EKeys::LeftShift,IE_Pressed);});Later(8.7f,[Key]{Key(EKeys::C,IE_Pressed);});
 // A duplicated press while sliding must not restart its timer or boost.
 Later(8.85f,[Key]{Key(EKeys::C,IE_Pressed);});
 Later(10.3f,[Key]{Key(EKeys::C,IE_Released);Key(EKeys::LeftShift,IE_Released);});
 Later(11,[Key]{Key(EKeys::SpaceBar,IE_Pressed);});Later(11.1f,[Key]{Key(EKeys::SpaceBar,IE_Released);});
 Later(12.8f,[this,Key]{SetActorLocation(FVector(-450,-350,95));GetCharacterMovement()->StopMovementImmediately();Key(EKeys::LeftShift,IE_Pressed);});
 Later(13.5f,[Key]{Key(EKeys::C,IE_Pressed);});Later(13.7f,[Key]{Key(EKeys::SpaceBar,IE_Pressed);});Later(13.8f,[Key]{Key(EKeys::SpaceBar,IE_Released);});
 Later(14.8f,[Key]{Key(EKeys::C,IE_Released);Key(EKeys::LeftShift,IE_Released);});
 Later(15,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(15.7f,[Key]{Key(EKeys::C,IE_Pressed);});
 Later(16,[this]{
  SetActorLocation(FVector(-450,-350,70));GetCharacterMovement()->StopMovementImmediately();
  auto* Roof=GetWorld()->SpawnActor<AStaticMeshActor>();Roof->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
  Roof->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Roof->SetActorLocation(FVector(-450,-350,165));Roof->SetActorScale3D(FVector(3,3,.4));Roof->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));Roof->SetLifeSpan(1.0f);
 });
 Later(16.2f,[Key]{Key(EKeys::SpaceBar,IE_Pressed);});Later(16.25f,[Key]{Key(EKeys::SpaceBar,IE_Released);});Later(16.5f,[Key]{Key(EKeys::C,IE_Released);});
 Later(18.2f,[Key]{Key(EKeys::SpaceBar,IE_Pressed);});Later(18.3f,[Key]{Key(EKeys::SpaceBar,IE_Released);});
 Later(20,[this,Key]{Fire();Key(EKeys::C,IE_Pressed);Key(EKeys::RightMouseButton,IE_Pressed);});Later(20.2f,[this]{Reload();});
 Later(23,[Key]{Key(EKeys::C,IE_Released);Key(EKeys::RightMouseButton,IE_Released);});
 Later(24,[this,Key]{SetActorLocation(FVector(-450,-350,95));GetCharacterMovement()->StopMovementImmediately();Key(EKeys::LeftShift,IE_Pressed);});
 Later(24.7f,[Key]{Key(EKeys::C,IE_Pressed);});Later(25.6f,[Key]{Key(EKeys::C,IE_Released);Key(EKeys::LeftShift,IE_Released);});
 const TArray<float> Times={3.7f,4.8f,6.6f,8.9f,9.2f,9.9f,11.25f,11.6f,11.85f,13.85f,14.25f,14.65f,16.8f,17.5f,18.45f,21.3f,23.6f,24.95f,25.8f,5.85f,7.7f};
 for(int I=0;I<Times.Num();++I)Later(Times[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/mobility-%02d.png"),I),true,false);});
 Later(27,[this]{FFileHelper::SaveStringToFile(MobilityAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("mobility-audit.csv")));FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::TickMobilityAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();
 // Tap just before landing, through the actual input path. It must cause one
 // additional grounded jump, never a mid-air double jump or held auto-jump.
 static bool BufferedTap=false;
 if(!BufferedTap&&T>18.5f&&T<19.2f&&GetVelocity().Z<-100&&GetActorLocation().Z<117)
 {
  auto* PC=CastChecked<APlayerController>(Controller);
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::SpaceBar,IE_Pressed,FPlatformTime::Cycles64()));
  FTimerHandle H;GetWorldTimerManager().SetTimer(H,[PC]{PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::SpaceBar,IE_Released,FPlatformTime::Cycles64()));},.015f,false);
  BufferedTap=true;
 }
 if((T>=6&&T<7)||(T>=8&&T<10.4)||(T>=12.8&&T<14.6)||(T>=24&&T<25.5))
  if(!bSliding)AddMovementInput(FVector(1,0,0));
 if(T<2.5||T>=27)return;
 const FVector L=GetActorLocation(),V=GetVelocity();
 const FVector Root=GetMesh()->GetSocketTransform(TEXT("Root"),RTS_Component).GetLocation();
 MobilityAuditSamples+=FString::Printf(TEXT("%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%.5f,%.5f,%.5f,%u,%u,%u,%.5f,%.5f,%d\n"),T,L.X,L.Y,L.Z,V.Size2D(),V.Z,bIsCrouched,bSliding,SlideBlend,AirBlend,LandingTime,Camera->GetComponentLocation().Z,GetMesh()->GetSocketLocation(TEXT("Head")).Z,GetMesh()->GetSocketLocation(TEXT("Foot.L")).Z,GetMesh()->GetSocketLocation(TEXT("Foot.R")).Z,Root.X,Root.Y,bReloading,Ammo,Locomotion.LocalVelocity.X,Locomotion.LocalVelocity.Y,Locomotion.DirectionDegrees,Locomotion.JumpSerial,Locomotion.LandingSerial,Locomotion.SlideSerial,Locomotion.LastSlideEntrySpeed,Locomotion.LastLandingSpeed,int32(Locomotion.Phase));
}
