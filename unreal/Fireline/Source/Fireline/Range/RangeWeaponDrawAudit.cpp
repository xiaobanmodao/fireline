#include "RangeGame.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

// Compare the rendered transition against an independently sampled draw clip.
// State variables alone cannot detect a stale hold pose in the mesh buffers.
void ARangeCharacter::StartWeaponDrawAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponDrawAudit")))return;
 struct FState
 {
  int32 Case=0,Serial=-1,Frame=0,Equips=0,Samples=0,Failures=0;
  FString Rows=TEXT("case,frame,third_person,action,time,draw_error_cm,draw_error_deg,hold_error_cm,visible\n");
 };
 auto State=MakeShared<FState>();
 int32 FPS=60;FParse::Value(FCommandLine::Get(),TEXT("DrawAuditFPS="),FPS);FPS=FMath::Clamp(FPS,30,120);
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/FPS);
 if(auto* PC=Cast<APlayerController>(Controller)){PC->SetIgnoreLookInput(true);PC->SetIgnoreMoveInput(true);}
 auto* Reference=NewObject<USkeletalMeshComponent>(this);
 AddInstanceComponent(Reference);Reference->SetSkeletalMesh(KarambitArmsMesh);
 Reference->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Reference->SetVisibility(false);Reference->SetHiddenInGame(true);
 Reference->RegisterComponent();Reference->SetComponentTickEnabled(false);
 auto* Draw=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Karambit/A_Karambit_Draw.A_Karambit_Draw"));
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Karambit/A_Karambit_Hold.A_Karambit_Hold"));
 auto* ViewDraw=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/FirstPerson/Karambit/A_Karambit_Draw.A_Karambit_Draw"));
 auto* ViewHold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/FirstPerson/Karambit/A_Karambit_Hold.A_Karambit_Hold"));
 check(Draw&&Hold&&ViewDraw&&ViewHold&&KarambitArmsMesh);
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Tap=[this](FKey Key)
 {
  auto* PC=CastChecked<APlayerController>(Controller);
  for(EInputEvent Event:{IE_Pressed,IE_Released})PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,FPlatformTime::Cycles64()));
 };
 Later(1,[this,State,Tap]{Controller->SetControlRotation(FRotator::ZeroRotator);State->Case=1;Tap(EKeys::Two);});
 Later(3,[Tap]{Tap(EKeys::Zero);});
 Later(3.7f,[State,Tap]{State->Case=2;Tap(EKeys::Two);});
 Later(6,[Tap]{Tap(EKeys::MouseScrollUp);});
 Later(6.08f,[State,Tap]{State->Case=3;Tap(EKeys::MouseScrollDown);});
 Later(9,[Tap]{Tap(EKeys::One);});
 Later(9.7f,[this,State,Tap]{ToggleView();Controller->SetControlRotation(FRotator(-8,150,0));State->Case=4;Tap(EKeys::Two);});
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,State,Reference,Draw,Hold,ViewDraw,ViewHold](UWorld* World,ELevelTick,float Dt)
 {
  if(World!=GetWorld()||!bKnife||HandSwitchLeft>.24f||KnifeTime>.35f)return;
  if(State->Serial!=KnifeActionSerial){State->Serial=KnifeActionSerial;State->Frame=0;++State->Equips;}
  if(KnifeAction!=1&&State->Frame>0)return;
  auto* Mesh=bThirdPerson?GetMesh():Arms.Get();
  const FName Bones[]={TEXT("LowerArm_R"),TEXT("LowerArm_L"),TEXT("DJ_wrist_R"),TEXT("DJ_wrist_L"),TEXT("DJ_index_01_R"),TEXT("HandHold_R")};
  auto Error=[&](UAnimSequence* Clip,float Time)
  {
   Reference->PlayAnimation(Clip,false);Reference->SetPosition(Time,false);
   Reference->TickAnimation(0,false);Reference->RefreshBoneTransforms();
   const FTransform ActualTorso=Mesh->GetSocketTransform(TEXT("Torso"),RTS_Component);
   const FTransform RefTorso=Reference->GetSocketTransform(TEXT("Torso"),RTS_Component);
   FVector2D Max=FVector2D::ZeroVector;
   for(const FName Bone:Bones)
   {
    if(!Mesh->DoesSocketExist(Bone)||!Reference->DoesSocketExist(Bone))return FVector2D(10000,180);
    const FTransform Actual=Mesh->GetSocketTransform(Bone,RTS_Component).GetRelativeTransform(ActualTorso);
    const FTransform Expected=Reference->GetSocketTransform(Bone,RTS_Component).GetRelativeTransform(RefTorso);
    Max.X=FMath::Max(Max.X,FVector::Distance(Actual.GetLocation(),Expected.GetLocation()));
    Max.Y=FMath::Max(Max.Y,FMath::RadiansToDegrees(Actual.GetRotation().AngularDistance(Expected.GetRotation())));
   }
   return Max;
  };
  UAnimSequence* DrawClip=bThirdPerson?Draw:ViewDraw;
  UAnimSequence* HoldClip=bThirdPerson?Hold:ViewHold;
  FVector2D DrawError=Error(DrawClip,KnifeTime);
  // Components can tick before the character advances action time (also the
  // view arms in cooked builds). Accept that frame's draw sample, never hold.
  const FVector2D Previous=Error(DrawClip,FMath::Max(0.f,KnifeTime-Dt));if(Previous.Y<DrawError.Y)DrawError=Previous;
  // Mesh initialization can publish the valid draw-start sample before its
  // normal tick. Either draw sample is valid; the hold pose never is.
  if(State->Frame==0){const FVector2D Initial=Error(DrawClip,0);if(Initial.X<DrawError.X)DrawError=Initial;}
  const FVector2D HoldError=Error(HoldClip,0);
  const bool Visible=bThirdPerson?BodyKnife->IsVisible():ViewKnife->IsVisible();
  const bool Pass=KnifeAction==1&&Visible&&DrawError.X<.02&&DrawError.Y<.05;
  ++State->Samples;State->Failures+=!Pass;
  State->Rows+=FString::Printf(TEXT("%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%d\n"),State->Case,State->Frame,bThirdPerson,KnifeAction,KnifeTime,DrawError.X,DrawError.Y,HoldError.X,Visible);
  if(!Pass||State->Frame==0)UE_LOG(LogTemp,Display,TEXT("DRAW_FRAME case=%d frame=%d time=%.6f draw_cm=%.6f draw_deg=%.6f hold_cm=%.6f pass=%d"),State->Case,State->Frame,KnifeTime,DrawError.X,DrawError.Y,HoldError.X,Pass);
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponDrawCapture"))&&(State->Case==1||State->Frame<4))
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/draw-transition-%d-%02d.png"),State->Case,State->Frame),true,false);
  ++State->Frame;
 });
 Later(12,[State,Reference,FPS]
 {
  FFileHelper::SaveStringToFile(State->Rows,*(FPaths::ProjectSavedDir()/TEXT("weapon-draw-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("WEAPON_DRAW_AUDIT fps=%d equips=%d samples=%d failures=%d"),FPS,State->Equips,State->Samples,State->Failures+(State->Equips!=4));
  Reference->DestroyComponent();FPlatformMisc::RequestExit(false);
 });
}
