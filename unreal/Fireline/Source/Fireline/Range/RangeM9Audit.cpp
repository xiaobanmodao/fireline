#include "RangeGame.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"

void ARangeCharacter::StartM9Audit()
{
 const bool InputAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineM9InputAudit"));
 bM9Audit=InputAudit||FParse::Param(FCommandLine::Get(),TEXT("FirelineM9Audit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineM9ContactAudit"));if(!bM9Audit)return;
 if(!InputAudit)if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("FirelineM9Capture"));
 if(Capture){FApp::SetFixedDeltaTime(1.0/60.0);FApp::SetUseFixedTimeStep(true);}
 M9AuditSamples=TEXT("time,knife,empty,third,action,action_time,serial,blend,switch,view_knife,view_gun,ammo,reload,aim,x,y,z\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 if(InputAudit)
 {
  auto InputLater=[this,Later](float T,TFunction<void()> Fn){Later(bKarambitReady?3.f+(T-3.f)*1.4f:T,MoveTemp(Fn));};
  // Press and release enter PlayerInput before the same frame is processed.
  // IsInputKeyDown is already false, so this specifically detects lost quick taps.
  auto Tap=[this](FKey Key){auto* PC=CastChecked<APlayerController>(Controller);const auto Device=FInputDeviceId::CreateFromInternalId(0);for(EInputEvent E:{IE_Pressed,IE_Released})PC->InputKey(FInputKeyEventArgs(nullptr,Device,Key,E,FPlatformTime::Cycles64()));};
  struct FTapState {bool Tap=false,Repeat=false,Interrupt=false,Keys=false;int32 SlashSerial=0;};auto State=MakeShared<FTapState>();
  InputLater(3,[Tap]{Tap(EKeys::Two);});InputLater(4.2f,[Tap]{Tap(EKeys::LeftMouseButton);});
  InputLater(4.3f,[this,State]{State->Tap=bKnife&&KnifeAction==3;State->SlashSerial=KnifeActionSerial;});
  InputLater(4.4f,[Tap]{Tap(EKeys::LeftMouseButton);Tap(EKeys::F);});InputLater(4.5f,[this,State]{State->Repeat=KnifeAction==3&&KnifeActionSerial==State->SlashSerial;});
  InputLater(5.3f,[Tap]{Tap(EKeys::F);});InputLater(5.55f,[Tap]{Tap(EKeys::LeftMouseButton);});
  InputLater(5.8f,[this,State]{State->Interrupt=KnifeAction==3&&KnifeActionSerial==State->SlashSerial+2;});
  InputLater(6.7f,[Tap]{Tap(EKeys::One);});InputLater(7.5f,[Tap]{Tap(EKeys::Two);});
  InputLater(8.6f,[this,State]{State->Keys=bKnife&&!bEmptyHands&&KnifeAction==0;});
  InputLater(9,[this,State]{const FString Report=FString::Printf(TEXT("{\"same_frame_tap\":%s,\"repeat_guard\":%s,\"inspect_interrupt\":%s,\"weapon_keys\":%s,\"ammo_unchanged\":%s}"),State->Tap?TEXT("true"):TEXT("false"),State->Repeat?TEXT("true"):TEXT("false"),State->Interrupt?TEXT("true"):TEXT("false"),State->Keys?TEXT("true"):TEXT("false"),Ammo==30?TEXT("true"):TEXT("false"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("m9-input-audit.json")));UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_INPUT_AUDIT %s"),*Report);FPlatformMisc::RequestExit(false);});
  return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineM9ContactAudit")))
 {
  struct FContactState {TWeakObjectPtr<ARangeTarget> Target;TWeakObjectPtr<AStaticMeshActor> Cover;bool Single=false,Blocked=false,Killed=false,Range=false;};
  auto State=MakeShared<FContactState>();TActorIterator<ARangeTarget> FirstTarget(GetWorld());if(FirstTarget)State->Target=*FirstTarget;check(State->Target.IsValid());
  auto Place=[this,State](float Distance)
  {
   GetCharacterMovement()->StopMovementImmediately();SetActorLocation(State->Target->GetActorLocation()+FVector(-Distance,0,0),false,nullptr,ETeleportType::TeleportPhysics);
   const FVector Point=State->Target->Body->GetSocketLocation(TEXT("Torso"))+FVector(0,0,25);
   Controller->SetControlRotation((Point-(GetActorLocation()+FVector(0,0,75))).Rotation());
  };
  Later(3,[this]{EquipKnife();});Later(4.3f,[Place]{Place(110);});
  Later(4.6f,[this]{Fire();Fire();Fire();});
  Later(5,[this,State]{State->Single=State->Target->GetHealth()==50&&Hits==1;});
  Later(5.6f,[this,State]{
   const FVector Point=State->Target->Body->GetSocketLocation(TEXT("Torso"))+FVector(0,0,25);
   auto* Cover=GetWorld()->SpawnActor<AStaticMeshActor>();auto* C=Cover->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);Cover->SetActorLocation((Camera->GetComponentLocation()+Point)*.5f);
   C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetCollisionProfileName(TEXT("BlockAll"));Cover->SetActorScale3D(FVector(.25,.7,.7));State->Cover=Cover;
  });
  Later(5.75f,[this]{Fire();});Later(6.2f,[State]{State->Blocked=State->Target->GetHealth()==50;if(State->Cover.IsValid())State->Cover->Destroy();});
  Later(6.8f,[this]{Fire();});Later(7.2f,[this,State]{State->Killed=State->Target->GetHealth()==0&&Kills==1&&Hits==2;});
  Later(9.6f,[Place]{Place(250);});Later(9.85f,[this]{Fire();});Later(10.4f,[State]{State->Range=State->Target->GetHealth()==100;});
  Later(11,[this,State]{
   const FString Report=FString::Printf(TEXT("{\"single_hit\":%s,\"cover_blocks\":%s,\"two_hit_elimination\":%s,\"range_limit\":%s,\"ammo_unchanged\":%s}"),State->Single?TEXT("true"):TEXT("false"),State->Blocked?TEXT("true"):TEXT("false"),State->Killed?TEXT("true"):TEXT("false"),State->Range?TEXT("true"):TEXT("false"),(Ammo==30&&Shots==0)?TEXT("true"):TEXT("false"));
   FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("m9-contact-audit.json")));UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_CONTACT_AUDIT %s"),*Report);FPlatformMisc::RequestExit(false);
  });
  return;
 }
 Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(4,[this]{EquipKnife();});
 Later(4.5f,[this]{const int32 Before=KnifeActionSerial;TestFingers();Fire();UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_DRAW_GUARD %d"),KnifeActionSerial==Before&&KnifeAction==1);});
 Later(5.5f,[this]{TestFingers();});
 for(float T:{5.8f,6.1f,6.4f})Later(T,[this]{const int32 Before=KnifeActionSerial;const float Time=KnifeTime;TestFingers();UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_REPEAT_GUARD %d"),KnifeActionSerial==Before&&KnifeTime==Time);});
 Later(9.15f,[this]{Fire();});
 Later(10.2f,[this]{TestFingers();});Later(11.1f,[this]{Fire();});
 Later(11.3f,[this]{const int32 Before=KnifeActionSerial;Fire();TestFingers();UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_SLASH_GUARD %d"),KnifeActionSerial==Before);});
 Later(12.3f,[this]{EquipRifle();});
 const bool Quick=FParse::Param(FCommandLine::Get(),TEXT("FirelineM9Quick"));
 const bool Audio=FParse::Param(FCommandLine::Get(),TEXT("FirelineM9AudioCapture"));
 // Audio runs without screenshot readback/fixed simulation. The recording uses
 // the same game-clock event schedule as the separate visual review capture.
 if(Audio)
 {
  check(!Capture);
  Later(3.5f,[this]{UAudioMixerBlueprintLibrary::StartRecordingOutput(this,33);});
  Later(Quick?13.3f:35.5f,[this]{UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("m9-gameplay"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));});
 }
 if(!Quick)
 {
  Later(13.3f,[this]{StartAim();});Later(14,[this]{EndAim();Ammo=29;Reload();});
  Later(17,[this]{EquipKnife();});
  Later(18.2f,[this]{ToggleView();Controller->SetControlRotation(FRotator(-8,155,0));});
  Later(18.5f,[this]{TestFingers();});Later(22.2f,[this]{Fire();});
  Later(23.2f,[this]{StartCrouch();});Later(24,[this]{TestFingers();});Later(25,[this]{Fire();});
  Later(26,[this]{EndCrouch();StartJump();});Later(26.12f,[this]{EndJump();});
  Later(27,[this]{EquipRifle();});Later(27.05f,[this]{ToggleEmptyHands();});Later(27.1f,[this]{EquipKnife();});
  Later(30,[this]{ToggleView();Controller->SetControlRotation(FRotator(55,90,0));TestFingers();});
  Later(31.2f,[this]{Controller->SetControlRotation(FRotator(-55,270,0));});
  Later(32.4f,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
  Later(34,[this]{EquipRifle();});
 }
 Later((Quick?13.3f:35.5f)+(Audio?.5f:0.f),[this]{
  FFileHelper::SaveStringToFile(M9AuditSamples,*(FPaths::ProjectSavedDir()/TEXT("m9-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_M9_AUDIT_COMPLETE rifle=%d ammo=%d"),!bKnife&&!bEmptyHands,Ammo);FPlatformMisc::RequestExit(false);
 });
}
void ARangeCharacter::TickM9Audit(float Dt)
{
 const float T=GetGameTimeSinceCreation();if(T<3.5f)return;
 if(T>28.6f&&T<29.6f)AddMovementInput(GetActorForwardVector(),.6f);
 const FVector P=Camera->GetComponentTransform().InverseTransformPosition(Arms->GetSocketLocation(TEXT("UpperHand_R")));
 M9AuditSamples+=FString::Printf(TEXT("%.6f,%d,%d,%d,%d,%.6f,%d,%.6f,%.6f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f\n"),T,bKnife,bEmptyHands,bThirdPerson,KnifeAction,KnifeTime,KnifeActionSerial,KnifeBlend,HandSwitchLeft,ViewKnife->IsVisible(),ViewGun->IsVisible(),Ammo,bReloading,AimAlpha,P.X,P.Y,P.Z);
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineM9Capture")))return;
 static int32 LastStep=-1;static int32 Index=0;const int32 Step=FMath::FloorToInt(T*30.f);
 if(Step!=LastStep){LastStep=Step;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/m9-%04d.png"),Index++),true,false);}
}
